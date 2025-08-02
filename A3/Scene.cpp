#include "Scene.h"

#include <fstream>

#include "Utility.h"
#include "MeshObject.h"
#include "MeshResource.h"
#include "CameraObject.h"
#include "RenderSettings.h"

#include "Json.hpp"

#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/tiny_gltf/tiny_gltf.h"

#if defined(_DEBUG)
#define MAX_LOG_BUFFER_LENGTH 2048
#define A3_LOG(developer, fmt, ...)														\
do {																					\
    char buffer[MAX_LOG_BUFFER_LENGTH];													\
    snprintf(buffer, sizeof(buffer), "[" developer "]" fmt, ##__VA_ARGS__);				\
    OutputDebugStringA(buffer);															\
    OutputDebugStringA("\n");															\
} while(0)

	// for assert
#define A3_ASSERT_COMPILE(condition, msg) static_assert(condition, msg)
#define A3_ASSERT_DEV(developer, condition, fmt, ...)									\
do {																					\
	if(!(condition))																	\
	{																					\
		A3_LOG(developer, fmt, __VA_ARGS__);											\
		__debugbreak();																	\
	}																					\
} while(0)
#else
#define A3_LOG(developer, fmt, ...)
#define A3_ASSERT_Compile(condition, msg)
#define A3_Assert_DEV(developer, conditionm fmt, ...)
#endif

using namespace A3;
using Json = nlohmann::json;

Scene::Scene()
	: bSceneDirty(true), bBufferUpdated(true), bPosUpdated(true)
{
	this->imgui_param = std::make_unique<imguiParam>();
}

Scene::~Scene() {
	for (auto& resource : resources) {
		delete resource.second;
	}
}

// 노드의 로컬 변환 행렬 얻기 (glm 사용)
Mat4x4 getNodeLocalTransform(const tinygltf::Node& node) {
	Mat4x4 local = Mat4x4::identity;
	if (!node.matrix.empty()) {
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				local[j][i] = static_cast<float>(node.matrix[i * 4 + j]);
			}
		}
	}
	else {
		if (!node.scale.empty()) {
			local[0][0] = static_cast<float>(node.scale[0]);
			local[1][1] = static_cast<float>(node.scale[1]);
			local[2][2] = static_cast<float>(node.scale[2]);
		}
		if (!node.rotation.empty()) {
			struct Quaternion {
				float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
			};
			auto quaternionToMatrix = [](const Quaternion& q)->Mat4x4{
				Mat4x4 mat = Mat4x4::identity;
				float xx = q.x * q.x;
				float xy = q.x * q.y;
				float xz = q.x * q.z;
				float xw = q.x * q.w;
				float yy = q.y * q.y;
				float yz = q.y * q.z;
				float yw = q.y * q.w;
				float zz = q.z * q.z;
				float zw = q.z * q.w;

				mat[0][0] = 1.0f - 2.0f * (yy + zz);
				mat[0][1] = 2.0f * (xy + zw);
				mat[0][2] = 2.0f * (xz - yw);
				mat[1][0] = 2.0f * (xy - zw);
				mat[1][1] = 1.0f - 2.0f * (xx + zz);
				mat[1][2] = 2.0f * (yz + xw);
				mat[2][0] = 2.0f * (xz + yw);
				mat[2][1] = 2.0f * (yz - xw);
				mat[2][2] = 1.0f - 2.0f * (xx + yy);

				return mat;
				};
			Quaternion quat = { static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]) };
			Mat4x4 rotMat = quaternionToMatrix(quat);
			local = mul(local, rotMat);

			//float cx = std::cos(angleRad.x); float sx = std::sin(angleRad.x); // pitch
			//float cy = std::cos(angleRad.y); float sy = std::sin(angleRad.y); // yaw
			//float cz = std::cos(angleRad.z); float sz = std::sin(angleRad.z); // roll

			//Mat3x3 rotX{
			//	1,  0,  0,
			//	0, cx, -sx,
			//	0, sx,  cx
			//};
			//Mat3x3 rotY{
			//	 cy, 0, sy,
			//	  0, 1, 0,
			//	-sy, 0, cy
			//};
			//Mat3x3 rotZ{
			//	cz, -sz, 0,
			//	sz,  cz, 0,
			//	 0,   0, 1
			//};
			//Mat3x3 rot = rotZ * rotY * rotX;
		}
		if (!node.translation.empty()) {
			Mat4x4 t = Mat4x4::identity;
			t[3][0] = static_cast<float>(node.translation[0]);
			t[3][1] = static_cast<float>(node.translation[1]);
			t[3][2] = static_cast<float>(node.translation[2]);
			
			local = mul(local, t);
		}
	}
	return local;
}
// 템플릿 함수 (glm::vec 기반으로 수정)
template <typename T>
std::vector<T> getBufferData(const tinygltf::Model& model, int accessorIndex) {
	std::vector<T> data;
	if (accessorIndex >= 0 && accessorIndex < model.accessors.size()) {
		const auto& accessor = model.accessors[accessorIndex];
		if (accessor.bufferView >= 0 && accessor.bufferView < model.bufferViews.size()) {
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			if (bufferView.buffer >= 0 && bufferView.buffer < model.buffers.size()) {
				const auto& buffer = model.buffers[bufferView.buffer];
				size_t byteStride = bufferView.byteStride ? bufferView.byteStride : tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
				size_t byteOffset = bufferView.byteOffset + accessor.byteOffset;
				size_t count = accessor.count;
				size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
				size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);

				data.resize(count);
				for (size_t i = 0; i < count; ++i) {
					const unsigned char* src = buffer.data.data() + byteOffset + i * byteStride;
					T* dst = reinterpret_cast<T*>(data.data()) + i;
					std::memcpy(dst, src, componentSize * numComponents);
				}
			}
		}
	}
	return data;
}

bool AreSame(const float a, const float b, const float epsilon)
{
	return fabs(a - b) < epsilon;
}
void Scene::loadGLTF(const std::string& fileName, VulkanRenderBackend& vulkanBackend)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool success = false;
	if (fileName.size() > 4 && fileName.substr(fileName.size() - 4) == ".glb")
	    success = loader.LoadBinaryFromFile(&model, &err, &warn, fileName);
	else 
		success = loader.LoadASCIIFromFile(&model, &err, &warn, fileName);

	if (!warn.empty()) 
		std::cerr << "WARN: " << warn << std::endl;

	if (!err.empty()) 
		std::cerr << "ERROR: " << err << std::endl;

	if (!success) 
	{
		std::cerr << "Failed to load glTF" << std::endl;
		return;
	}

	std::cout << "glTF file loaded successfully." << std::endl;

	if (model.skins.empty() == false)
	{
		std::map<int, int> nodeParents;
		for (int i = 0; i < model.nodes.size(); ++i) {
			for (int childIndex : model.nodes[i].children) {
				nodeParents[childIndex] = i;
			}
		}

		_skeletons.reserve(model.skins.size());
		for (const auto& skin : model.skins) {
			Skeleton skeleton;
			skeleton._skinName = skin.name.empty() ? "(unnamed)" : skin.name;

			std::vector<Mat4x4> inverseBindMatrices;
			if (skin.inverseBindMatrices >= 0) {
				const auto& ibmAccessor = model.accessors[skin.inverseBindMatrices];
				if (ibmAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && ibmAccessor.type == TINYGLTF_TYPE_MAT4) {
					inverseBindMatrices = getBufferData<Mat4x4>(model, skin.inverseBindMatrices);
				}
				else {
					std::cerr << "  Warning: Inverse Bind Matrices accessor has unexpected type for skin: " << skeleton._skinName << std::endl;
				}
			}

			for (Mat4x4& inv : inverseBindMatrices)
			{
				inv.transpose();
			}

			std::unordered_map<int, size_t> nodeIndexToBoneIndexMap;
			for (size_t i = 0; i < skin.joints.size(); ++i) {
				int jointIndex = skin.joints[i];
				if (jointIndex >= 0 && jointIndex < model.nodes.size()) {
					const auto& jointNode = model.nodes[jointIndex];
					std::string boneName = jointNode.name.empty() ? ("joint_" + std::to_string(jointIndex)) : jointNode.name;
					Mat4x4 bindPoseLocalMatrix = getNodeLocalTransform(jointNode);

					bindPoseLocalMatrix.transpose();

					Mat4x4 bindPoseWorldMatrix = bindPoseLocalMatrix;
					int tempNodeIndex = jointIndex;
					while (nodeParents.count(tempNodeIndex))
					{
						int parentNodeIndex = nodeParents.at(tempNodeIndex);
						Mat4x4 parentLocalMatrix = getNodeLocalTransform(model.nodes[parentNodeIndex]);
						parentLocalMatrix.transpose();
						bindPoseWorldMatrix = mul(parentLocalMatrix, bindPoseWorldMatrix);
						tempNodeIndex = parentNodeIndex;
					}

					Bone bone;
					bone._bonaName = boneName;
					bone._childBoneIndexArr = jointNode.children;

					skeleton._boneArray.push_back(std::move(bone));

					nodeIndexToBoneIndexMap[jointIndex] = skeleton._boneArray.size() - 1;
					skeleton._boneNameIndexMapper[boneName] = static_cast<uint32>(skeleton._boneArray.size() - 1);

					skeleton._boneDressPoseArray.push_back(bindPoseWorldMatrix);
					if (i < inverseBindMatrices.size()) {
						skeleton._boneDressPoseInverseArray.push_back(inverseBindMatrices[i]);
					}
					else {
						std::cerr << "  Warning: No Inverse Bind Matrix found for bone: " << boneName << " in skin: " << skeleton._skinName << std::endl;
						skeleton._boneDressPoseInverseArray.push_back(Mat4x4::identity);
					}
				}
				else {
					std::cerr << "  Error: Invalid joint index: " << jointIndex << " in skin: " << skeleton._skinName << std::endl;
				}
			}

			for (int i = 0; i < skeleton._boneArray.size(); ++i)
			{
				if (skeleton._boneArray[i]._childBoneIndexArr.empty())
					continue;

				for (int j = 0; j < skeleton._boneArray[i]._childBoneIndexArr.size(); ++j)
				{
					const int childJointIndex = skeleton._boneArray[i]._childBoneIndexArr[j];
					const int childBoneIndex = static_cast<int>(nodeIndexToBoneIndexMap.find(childJointIndex)->second);

					A3_ASSERT_DEV("LongtimeProdigy", i < childBoneIndex, "遺紐?蹂??몃뜳??%d)??諛섎뱶???먯떇 蹂??몃뜳??%d)蹂대떎 ?묒븘???⑸땲??", i, childBoneIndex);

					skeleton._boneArray[childBoneIndex]._parentBoneIndex = i;
					skeleton._boneArray[i]._childBoneIndexArr[j] = childBoneIndex;
				}
			}

			{
				std::cout << "\n--- Loaded Skeletons (glm) ---" << std::endl;
				std::cout << "Skin Name: " << skeleton._skinName << std::endl;
				for (size_t i = 0; i < skeleton._boneArray.size(); ++i) {
					std::cout << "  Bone Name: " << skeleton._boneArray[i]._bonaName;
					if (skeleton._boneArray[i]._parentBoneIndex >= 0)
					{
						std::cout << " (Parent: " << skeleton._boneArray[skeleton._boneArray[i]._parentBoneIndex]._bonaName << ")";
					}
					std::cout << std::endl;
				}
				std::cout << std::endl;
			}

			_skeletons.push_back(std::move(skeleton));
		}

		std::cout << "\n--- Animations ---" << std::endl;
		if (!model.animations.empty()) {
			for (uint32 i = 0; i < model.animations.size(); ++i)
			{
				Animation animation;

				const auto& anim = model.animations[i];
				std::cout << "\n--- Animation: " << (anim.name.empty() ? "(unnamed)" : anim.name) << " ---" << std::endl;

				float maxTime = 0.0f;
				for (const auto& channel : anim.channels)
				{
					std::string boneName = model.nodes[channel.target_node].name.empty() ? ("joint_" + std::to_string(channel.target_node)) : model.nodes[channel.target_node].name;
					AnimationData animData;

					std::string targetPath = channel.target_path;
					int samplerIndex = channel.sampler;

					if (samplerIndex < 0 || samplerIndex >= anim.samplers.size())
						continue;

					const auto& sampler = anim.samplers[samplerIndex];
					if (sampler.input < 0 || sampler.input >= model.accessors.size() || sampler.output < 0 || sampler.output >= model.accessors.size())
						continue;

					const auto& timeAccessor = model.accessors[sampler.input];
					const auto& valueAccessor = model.accessors[sampler.output];
					std::string lerpMethod = sampler.interpolation;

					for (const auto& maxValue : timeAccessor.maxValues)
						maxTime = std::max(maxTime, static_cast<float>(maxValue));

					std::vector<float> times;
					if (timeAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && timeAccessor.type == TINYGLTF_TYPE_SCALAR)
						times = getBufferData<float>(model, sampler.input);

					if (valueAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
						continue;

					enum class AnimationNodeType
					{
						TRANSLATION,
						ROTATION,
						SCALE,
						COUNT
					};
					AnimationNodeType type = AnimationNodeType::COUNT;
					if (valueAccessor.type == TINYGLTF_TYPE_VEC3 && targetPath == "translation")
					{
						animData._translation._data = getBufferData<Vec3>(model, sampler.output);
						animData._translation._time = times;
						type = AnimationNodeType::TRANSLATION;
					}
					else if (valueAccessor.type == TINYGLTF_TYPE_VEC4 && targetPath == "rotation")
					{
						animData._rotation._data = getBufferData<Vec4>(model, sampler.output);
						animData._rotation._time = times;
						type = AnimationNodeType::ROTATION;
					}
					else if (valueAccessor.type == TINYGLTF_TYPE_VEC3 && targetPath == "scale")
					{
						animData._scale._data = getBufferData<Vec3>(model, sampler.output);
						for (auto& scale : animData._scale._data)
						{
							scale = Vec3(1, 1, 1);
						}
						animData._scale._time = times;
						type = AnimationNodeType::SCALE;
					}

					const auto iter = animation._animData.find(boneName);
					if (iter == animation._animData.end())
					{
						animation._animData.insert(std::make_pair(boneName, animData));
					}
					else
					{
						AnimationData& data = iter->second;

						switch (type)
						{
						case AnimationNodeType::TRANSLATION:
						{
							A3_ASSERT_DEV("LongtimeProdigy", data._translation._data.empty(), "");
							A3_ASSERT_DEV("LongtimeProdigy", data._translation._time.empty(), "");
							data._translation = animData._translation;
							break;
						}
						case AnimationNodeType::ROTATION:
						{
							A3_ASSERT_DEV("LongtimeProdigy", data._rotation._data.empty(), "");
							A3_ASSERT_DEV("LongtimeProdigy", data._rotation._time.empty(), "");
							data._rotation = animData._rotation;
							break;
						}
						case AnimationNodeType::SCALE:
						{
							A3_ASSERT_DEV("LongtimeProdigy", data._scale._data.empty(), "");
							A3_ASSERT_DEV("LongtimeProdigy", data._scale._time.empty(), "");
							data._scale = animData._scale;
							break;
						}
						}
					}
				}

				animation._totalTime = maxTime;

				_animations.push_back(animation);
			}
		}
	}

    std::cout << "========================================" << std::endl;
    std::cout << "Materials Count: " << model.materials.size() << std::endl;
    std::cout << "========================================" << std::endl;
	for (size_t i = 0; i < model.materials.size(); ++i)
	{
		MaterialParameter materialParameter;

		const auto& material = model.materials[i];

		std::cout << "\n--- Material [" << i << "] ---" << std::endl;
		std::cout << "Name: " << (material.name.empty() ? "(No Name)" : material.name) << std::endl;

		// PBR Metallic-Roughness 워크플로우 정보 추출
		const auto& pbr = material.pbrMetallicRoughness;

		// 1. Base Color
		std::cout << "  PBR Base Color Factor: [" << pbr.baseColorFactor[0] << ", " << pbr.baseColorFactor[1] << ", " << pbr.baseColorFactor[2] << ", " << pbr.baseColorFactor[3] << "]" << std::endl;
		materialParameter._baseColorFactor = Vec4(static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]), static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3]));

		auto ConvertUcharToFloat = [](const tinygltf::Image& image) -> std::vector<float>
			{
				const std::vector<unsigned char>& uchar_data = image.image;

				if (uchar_data.empty())
					return {};

				std::vector<float> float_data;
				float_data.reserve(uchar_data.size());

				for (unsigned char val : uchar_data)
					float_data.push_back(static_cast<float>(val) / 255.0f);

				return float_data;
			};

		if (pbr.baseColorTexture.index >= 0)
		{
			const auto& texture = model.textures[pbr.baseColorTexture.index];
			const auto& image = model.images[texture.source];
			std::cout << "  PBR Base Color Texture: " << image.uri << " (TexCoord: " << pbr.baseColorTexture.texCoord << ")" << std::endl;

			materialParameter._baseColorTexture = TextureManager::createTexture(vulkanBackend, image.uri, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
		}

		// 2. Metallic & Roughness
		std::cout << "  PBR Metallic Factor: " << pbr.metallicFactor << std::endl;
		std::cout << "  PBR Roughness Factor: " << pbr.roughnessFactor << std::endl;
		materialParameter._metallicFactor = static_cast<float>(pbr.metallicFactor);
		materialParameter._roughnessFactor = static_cast<float>(pbr.roughnessFactor);

		if (pbr.metallicRoughnessTexture.index >= 0)
		{
			const auto& texture = model.textures[pbr.metallicRoughnessTexture.index];
			const auto& image = model.images[texture.source];
			std::cout << "  PBR Metallic-Roughness Texture: " << image.uri << " (TexCoord: " << pbr.metallicRoughnessTexture.texCoord << ")" << std::endl;

			materialParameter._metallicRoughnessTexture = TextureManager::createTexture(vulkanBackend, image.name, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
		}

		// 3. Normal Texture
		if (material.normalTexture.index >= 0) 
		{
			const auto& texture = model.textures[material.normalTexture.index];
			const auto& image = model.images[texture.source];
			std::cout << "  Normal Texture: " << image.uri << " (Scale: " << material.normalTexture.scale << ", TexCoord: " << material.normalTexture.texCoord << ")" << std::endl;

			materialParameter._normalTexture = TextureManager::createTexture(vulkanBackend, image.name, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
		}

		// 4. Occlusion Texture
		if (material.occlusionTexture.index >= 0) 
		{
			const auto& texture = model.textures[material.occlusionTexture.index];
			const auto& image = model.images[texture.source];
			std::cout << "  Occlusion Texture: " << image.uri << " (Strength: " << material.occlusionTexture.strength << ", TexCoord: " << material.occlusionTexture.texCoord << ")" << std::endl;

			materialParameter._occlusionTexture = TextureManager::createTexture(vulkanBackend, image.name, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
		}

		// 5. Emissive
		std::cout << "  Emissive Factor: [" << material.emissiveFactor[0] << ", " << material.emissiveFactor[1] << ", " << material.emissiveFactor[2] << "]" << std::endl;
		materialParameter._emissiveFactor = Vec3(static_cast<float>(material.emissiveFactor[0]), static_cast<float>(material.emissiveFactor[1]), static_cast<float>(material.emissiveFactor[2]));

		if (material.emissiveTexture.index >= 0) 
		{
			const auto& texture = model.textures[material.emissiveTexture.index];
			const auto& image = model.images[texture.source];
			std::cout << "  Emissive Texture: " << image.uri << " (TexCoord: " << material.emissiveTexture.texCoord << ")" << std::endl;

			materialParameter._emissiveTexture = TextureManager::createTexture(vulkanBackend, image.name, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
		}

		// Transmission (KHR_materials_transmission)
		if (material.extensions.count("KHR_materials_transmission") > 0) 
		{
			const auto& transmission = material.extensions.find("KHR_materials_transmission")->second;
			if (transmission.Has("transmissionFactor")) 
			{
				materialParameter._transmissionFactor = static_cast<float>(transmission.Get("transmissionFactor").GetNumberAsDouble());
				std::cout << "  Transmission Factor: " << materialParameter._transmissionFactor << std::endl;
			}
			if (transmission.Has("transmissionTexture")) 
			{
				const auto& transmissionTextureInfo = transmission.Get("transmissionTexture");
				int textureIndex = transmissionTextureInfo.Get("index").GetNumberAsInt();
				if (textureIndex >= 0 && textureIndex < model.textures.size()) 
				{
					const auto& texture = model.textures[textureIndex];
					const auto& image = model.images[texture.source];
					std::cout << "  Transmission Texture: " << image.uri << std::endl;
					materialParameter._transmissionTexture = TextureManager::createTexture(vulkanBackend, image.name, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
				}
			}
		}

		// IOR (KHR_materials_ior)
		if (material.extensions.count("KHR_materials_ior") > 0) 
		{
			const auto& ior = material.extensions.find("KHR_materials_ior")->second;
			if (ior.Has("ior")) 
			{
				materialParameter._ior = static_cast<float>(ior.Get("ior").GetNumberAsDouble());
				std::cout << "  IOR: " << materialParameter._ior << std::endl;
			}
		}

		//// 7. Alpha Mode
		//std::cout << "  Alpha Mode: " << material.alphaMode << std::endl;
		//if (material.alphaMode == "MASK") 
		//{
		//	std::cout << "  Alpha Cutoff: " << material.alphaCutoff << std::endl;
		//}

		//// 8. Double Sided
		//std::cout << "  Double Sided: " << (material.doubleSided ? "True" : "False") << std::endl;

		
		// ======== using sheen material ========
		// ======================================
		materialParameter._sheenColorFactor = Vec3(0);
		materialParameter._sheenRoughnessFactor = 0.f;
		if (material.extensions.find("KHR_materials_sheen") != material.extensions.end())
		{
			// find sheen extension
			const auto& sheen = material.extensions.at("KHR_materials_sheen");

			// sheen color
			if (sheen.Has("sheenColorFactor"))
			{
				const auto& sheenColor = sheen.Get("sheenColorFactor");
				if (sheenColor.IsArray() && sheenColor.Size() >= 3)
				{
					float R = static_cast<float>(sheenColor.Get(0).GetNumberAsDouble());
					float G = static_cast<float>(sheenColor.Get(1).GetNumberAsDouble());
					float B = static_cast<float>(sheenColor.Get(2).GetNumberAsDouble());

					std::cout << "  Sheen Color Factor: " << R << ", " << G << ", " << B << std::endl;
					materialParameter._sheenColorFactor = Vec3(R, G, B);
				}
			}

			// sheen Roughness
			if (sheen.Has("sheenRoughnessFactor"))
			{
				const auto& sheenRoughness = static_cast<float>(sheen.Get("sheenRoughnessFactor").GetNumberAsDouble());
				std::cout << "  sheen roughness Factor : " << sheenRoughness << std::endl;
				materialParameter._sheenRoughnessFactor = sheenRoughness;
			}

			// sheen Color Texture
			if (sheen.Has("sheenColorTexture"))
			{
				const auto& sheenColorTexture = sheen.Get("sheenColorTexture");
				if (sheenColorTexture.Has("index"))
				{
					int index = sheenColorTexture.Get("index").Get<int>();
					const auto& texture = model.textures[index];
					const auto& image = model.images[texture.source];
					std::cout << "  Sheen Color Texture: " << image.uri << std::endl;

					materialParameter._sheenColorTexture = TextureManager::createTexture( vulkanBackend, image.uri, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data()
					);
				}
			}

			// sheen Roughness Texture
			if (sheen.Has("sheenRoughnessTexture"))
			{
				const auto& sheenRoughnessTexture = sheen.Get("sheenRoughnessTexture");
				if (sheenRoughnessTexture.Has("index"))
				{
					int index = sheenRoughnessTexture.Get("index").Get<int>();
					const auto& texture = model.textures[index];
					const auto& image = model.images[texture.source];
					std::cout << "  Sheen Roughness Texture: " << image.uri << std::endl;

					materialParameter._sheenRoughnessTexture = TextureManager::createTexture(vulkanBackend,image.uri,static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), image.width, image.height, ConvertUcharToFloat(image).data());
				}
			}
		}
		// ======================================
		
		Material a3Material;
		a3Material._parameter = std::move(materialParameter);
		a3Material._buffer = vulkanBackend.createResourceBuffer(sizeof(a3Material._parameter), static_cast<const void*>(&a3Material._parameter));

		materialArr.push_back(std::move(a3Material));
	}

	std::cout << "\n--- Mesh Data (glm) ---" << std::endl;
	for (const auto& mesh : model.meshes)
	{
		std::cout << "Mesh: " << mesh.name << std::endl;
		for (const auto& primitive : mesh.primitives) {
			std::cout << "  Primitive:" << std::endl;

			auto getVec3Attribute = [&](const std::string& attributeName) {
				if (primitive.attributes.count(attributeName)) {
					int accessorIndex = primitive.attributes.at(attributeName);
					if (accessorIndex >= 0 && accessorIndex < model.accessors.size()) {
						const auto& accessor = model.accessors[accessorIndex];
						if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3) {
							return getBufferData<Vec3>(model, accessorIndex);
						}
						else {
							std::cerr << "    Unsupported " << attributeName << " data type." << std::endl;
						}
					}
				}
				return std::vector<Vec3>();
				};
			auto getVec2Attribute = [&](const std::string& attributeName) {
				if (primitive.attributes.count(attributeName)) {
					int accessorIndex = primitive.attributes.at(attributeName);
					if (accessorIndex >= 0 && accessorIndex < model.accessors.size()) {
						const auto& accessor = model.accessors[accessorIndex];
						if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC2) {
							return getBufferData<Vec2>(model, accessorIndex);
						}
						else {
							std::cerr << "    Unsupported " << attributeName << " data type." << std::endl;
						}
					}
				}
				return std::vector<Vec2>();
				};
			auto getVec4Attribute = [&](const std::string& attributeName, int componentType) {
				if (primitive.attributes.count(attributeName)) {
					int accessorIndex = primitive.attributes.at(attributeName);
					if (accessorIndex >= 0 && accessorIndex < model.accessors.size()) {
						const auto& accessor = model.accessors[accessorIndex];
						if (accessor.type == TINYGLTF_TYPE_VEC4) {
							if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
								return getBufferData<Vec4>(model, accessorIndex);
							}
							else {
								std::cerr << "    Unsupported component type for " << attributeName << std::endl;
							}
						}
						else {
							std::cerr << "    Unsupported type for " << attributeName << std::endl;
						}
					}
				}
				return std::vector<Vec4>();
				};
			auto getIVec4Attribute = [&](const std::string& attributeName) {
				if (primitive.attributes.count(attributeName)) {
					int accessorIndex = primitive.attributes.at(attributeName);
					if (accessorIndex >= 0 && accessorIndex < model.accessors.size()) {
						const auto& accessor = model.accessors[accessorIndex];
						if (accessor.type == TINYGLTF_TYPE_VEC4) {
							if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
								struct ShortVec4
								{
									short x;
									short y;
									short z;
									short w;
								};
								std::vector<ShortVec4> data = getBufferData<ShortVec4>(model, accessorIndex);
								std::vector<IVec4> converted(data.size());
								for (size_t i = 0; i < data.size(); ++i) {
									converted[i] = IVec4(data[i].x, data[i].y, data[i].z, data[i].w);
								}
								return converted;
							}
							else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
								struct UintByteVec4
								{
									uint8 x;
									uint8 y;
									uint8 z;
									uint8 w;
								};
								std::vector<UintByteVec4> data = getBufferData<UintByteVec4>(model, accessorIndex);
								std::vector<IVec4> converted(data.size());
								for (size_t i = 0; i < data.size(); ++i) {
									converted[i] = IVec4(data[i].x, data[i].y, data[i].z, data[i].w);
								}
								return converted;
							}
							else {
								std::cerr << "    Unsupported component type for " << attributeName << std::endl;
							}
						}
						else {
							std::cerr << "    Unsupported type for " << attributeName << std::endl;
						}
					}
				}
				return std::vector<IVec4>();
				};

			std::vector<Vec3> positions = getVec3Attribute("POSITION");
			std::vector<Vec3> normals = getVec3Attribute("NORMAL");
			std::vector<Vec2> uvs = getVec2Attribute("TEXCOORD_0");

			std::vector<Vec4> weightsData;
			std::vector<IVec4> skinningBoneIndices;
			if (_skeletons.empty() == false)
			{
				const Skeleton& skeleton = _skeletons[0];

				weightsData = getVec4Attribute("WEIGHTS_0", TINYGLTF_COMPONENT_TYPE_FLOAT);
				std::vector<IVec4> jointIndicesData = getIVec4Attribute("JOINTS_0");
				skinningBoneIndices.resize(jointIndicesData.size());
				for (uint32 vertexIndex = 0; vertexIndex < jointIndicesData.size(); ++vertexIndex)
				{
					{
						const uint32 skeletonJointIndex = jointIndicesData[vertexIndex].x;
						const uint32 nodeIndex = model.skins[0].joints[skeletonJointIndex];
						const auto& jointNode = model.nodes[nodeIndex];
						A3_ASSERT_DEV("LongtimeProdigy", jointNode.name.empty() == false, "");
						std::string boneName = jointNode.name;
						const uint32 boneIndex = skeleton._boneNameIndexMapper.find(boneName)->second;
						skinningBoneIndices[vertexIndex].x = boneIndex;
					}
					{
						const uint32 skeletonJointIndex = jointIndicesData[vertexIndex].y;
						const uint32 nodeIndex = model.skins[0].joints[skeletonJointIndex];
						const auto& jointNode = model.nodes[nodeIndex];
						A3_ASSERT_DEV("LongtimeProdigy", jointNode.name.empty() == false, "");
						std::string boneName = jointNode.name;
						const uint32 boneIndex = skeleton._boneNameIndexMapper.find(boneName)->second;
						skinningBoneIndices[vertexIndex].y = boneIndex;
					}
					{
						const uint32 skeletonJointIndex = jointIndicesData[vertexIndex].z;
						const uint32 nodeIndex = model.skins[0].joints[skeletonJointIndex];
						const auto& jointNode = model.nodes[nodeIndex];
						A3_ASSERT_DEV("LongtimeProdigy", jointNode.name.empty() == false, "");
						std::string boneName = jointNode.name;
						const uint32 boneIndex = skeleton._boneNameIndexMapper.find(boneName)->second;
						skinningBoneIndices[vertexIndex].z = boneIndex;
					}
					{
						const uint32 skeletonJointIndex = jointIndicesData[vertexIndex].w;
						const uint32 nodeIndex = model.skins[0].joints[skeletonJointIndex];
						const auto& jointNode = model.nodes[nodeIndex];
						A3_ASSERT_DEV("LongtimeProdigy", jointNode.name.empty() == false, "");
						std::string boneName = jointNode.name;
						const uint32 boneIndex = skeleton._boneNameIndexMapper.find(boneName)->second;
						skinningBoneIndices[vertexIndex].w = boneIndex;
					}
				}
			}

			std::cout << "    Loaded " << positions.size() << " vertices." << std::endl;

			std::vector<uint32> indices;
			if (primitive.indices >= 0) {
				const auto& accessor = model.accessors[primitive.indices];
				//std::cout << "    Loading indices (count: " << accessor.count << ", type: " << tinygltf::GetComponentTypeString(accessor.componentType) << ")" << std::endl;
				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					std::vector<uint16_t> data = getBufferData<uint16_t>(model, primitive.indices);
					indices.assign(data.begin(), data.end());
				}
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					indices = getBufferData<uint32_t>(model, primitive.indices);
				}
				else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
					std::vector<uint8_t> data = getBufferData<uint8_t>(model, primitive.indices);
					indices.assign(data.begin(), data.end());
				}
				else {
					std::cerr << "    Unsupported index component type." << std::endl;
				}
			}
			std::cout << "    Loaded " << indices.size() << " indices." << std::endl;

			std::vector<VertexPosition> vPositions;
			std::vector<VertexAttributes> vAttributes;
			vPositions.reserve(positions.size());
			vAttributes.reserve(positions.size());
			
			for (int i = 0; i < positions.size(); ++i)
			{
				vPositions.push_back(VertexPosition(positions[i].x, positions[i].y, positions[i].z, 1.0f));
				vAttributes.push_back(VertexAttributes({ normals[i].x, normals[i].y, normals[i].z, 0.0f }, { uvs[i].x, uvs[i].y, 0.0f, 0.0f }));
			}

			if (resources.find(mesh.name) == resources.end()) {
				MeshResource resource;
				resource.positions = std::move(vPositions);
				resource.attributes = std::move(vAttributes);
				resource.indices = std::move(indices);
				resource._jointIndicesData = std::move(skinningBoneIndices);
				resource._weightsData = std::move(weightsData);
				resource.triangleCount = static_cast<uint32>(resource.indices.size() / 3);
				resource.cumulativeTriangleArea.resize(resource.triangleCount + 1);
				resource.cumulativeTriangleArea[0] = 0;
				for (uint32 triIndex = 1; triIndex < resource.triangleCount; ++triIndex)
				{
					const auto p1 = /*localToWorld * */resource.positions[resource.indices[triIndex * 3 + 0]];
					const auto p2 = /*localToWorld * */resource.positions[resource.indices[triIndex * 3 + 1]];
					const auto p3 = /*localToWorld * */resource.positions[resource.indices[triIndex * 3 + 2]];

					const auto v2_1 = p2 - p1;
					const auto v3_1 = p3 - p1;
					const auto normal = cross(v2_1, v3_1);
					const float magnitude = 0.5f * std::sqrt(dot(normal, normal));

					resource.cumulativeTriangleArea[triIndex] = resource.cumulativeTriangleArea[triIndex - 1] + magnitude;
				}

				resources[mesh.name] = new MeshResource(resource);
			}

			// TODO: change scale & pos in IMGUI
			static const float scale = 7.0f;
			static const Vec3 pos = Vec3(0, -5, 0);

			MeshObject* mo = new MeshObject(resources[mesh.name], &materialArr[primitive.material]);
			mo->setPosition(pos);
			mo->setScale(scale);
			mo->_skeletonIndex = 0;
			mo->_animationIndex = 0;
			this->objects.emplace_back(mo);
		}
	}
}

void Scene::load(const std::string& path, VulkanRenderBackend& vulkanBackend) {
	this->resources.clear();
	this->materialArr.clear();
	this->lightIndex.clear();
	this->objects.clear();

	std::ifstream file(path);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!: " + path);
	}

	Json data = Json::parse(file);

	auto& camera = data["camera"];
	if (camera.is_object()) {
		auto& position = camera["position"];
		auto& rotation = camera["rotation"];
		auto& fov = camera["yFovDeg"];
		auto& resolution = camera["resolution"]; // TODO: RenderSeetings.h -> runtime
		auto& sampling = camera["sampling"];
		auto& lightSampling = camera["lightSampling"];
		auto& maxDepth = camera["maxDepth"];
		auto& spp = camera["spp"];
		auto& exposure = camera["exposure"]; // TODO: add logic

		this->camera = std::make_unique<CameraObject>();
		this->camera->setPosition(Vec3(position[0], position[1], position[2]));
		this->camera->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
		this->camera->initializeFrontFromLocalToWorld();
		this->camera->setFov(fov);
		this->camera->setExposure(exposure);

		this->imgui_param->frameCount = spp; // one sampling per frame
		this->imgui_param->maxDepth = maxDepth;
		this->imgui_param->lightSamplingMode = (sampling == "bruteforce" ? imguiParam::BruteForce : imguiParam::NEE);
		this->imgui_param->lightSelection = (lightSampling == "light_only" ? imguiParam::LightOnly : imguiParam::EnvMap);
	}

	auto& envMap = data["envmap"];
	if (envMap.is_object()) {
		auto& envMapPath = envMap["image"];
		auto& envmapRotation = envMap["rotation"];			// TODO: add logic
		auto& envmapEmittance = envMap["emittanceScale"];	// TODO: add logic

		RenderSettings::envMapPath = "../Assets/" + static_cast<std::string>(envMapPath);
		this->imgui_param->envmapRotDeg = envmapRotation;
	}

#if 0 // Will be deprecated: only read gltf models
	auto& materials = data["materials"];

	auto& objects = data["sceneComponets"];
	if (objects.is_object()) {
		int index = 0;
		materialArrForObj.reserve(100);
		for (auto& [name, object] : objects.items())
		{
			auto& position = object["position"];
			auto& rotation = object["rotation"];
			auto& scale = object["scale"];
			auto& mesh = object["mesh"];
			std::string materialName = object["material"].get<std::string>();
			auto& material = materials[materialName];
			auto& baseColor = material["baseColor"];
			auto& metallic = material["metallic"];
			auto& subsurface = material["subsurface"];
			auto& specular = material["specular"];
			auto& roughness = material["roughness"];
			auto& specularTint = material["specularTint"];
			auto& anisotropic = material["anisotropic"];
			auto& sheen = material["sheen"];
			auto& sheenTint = material["sheenTint"];
			auto& clearcoat = material["clearcoat"];
			auto& clearcoatGloss = material["clearcoatGloss"];

			if (resources.find(mesh) == resources.end()) {
				MeshResource resource;
				Utility::loadMeshFile(resource, "../Assets/" + ((std::string)mesh));
				resources[mesh] = new MeshResource(resource);
			}

			MaterialParameter materialParameter;
			if (materialName == "light") 
			{
				materialParameter._emissiveFactor = Vec3(material["emittance"]);
			}
			else
			{
				materialParameter._metallicFactor = metallic.get<float>();
				materialParameter._roughnessFactor = roughness.get<float>();
			}
			materialParameter._baseColorFactor = Vec4(baseColor[0], baseColor[1], baseColor[2], 1);

			Material a3Material;
			a3Material._parameter = materialParameter;
			a3Material._buffer = vulkanBackend.createResourceBuffer(sizeof(a3Material._parameter), static_cast<const void*>(&a3Material._parameter));
			materialArrForObj.push_back(a3Material);

			MeshObject* mo = new MeshObject(resources[mesh], &materialArrForObj[materialArrForObj.size() - 1]);
			mo->setPosition(Vec3(position[0], position[1], position[2]));
			mo->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
			mo->setScale(Vec3(scale[0], scale[1], scale[2]));
			mo->calculateTriangleArea();

			if (materialName == "light") {
				mo->setBaseColor(Vec3(baseColor[0], baseColor[1], baseColor[2]));
				lightIndex.push_back(index);
				auto& emittance = material["emittance"];
				mo->setEmittance(emittance);
			}
			else {
				DisneyMaterial mat;
				mat.baseColor = Vec3(baseColor[0], baseColor[1], baseColor[2]);
				mat.metallic = metallic;
				mat.roughness = roughness;
				mat.subsurface = subsurface;
				mat.specular = specular;
				mat.specularTint = specularTint;
				mat.anisotropic = anisotropic;
				mat.sheen = sheen;
				mat.sheenTint = sheenTint;
				mat.clearcoat = clearcoat;
				mat.clearcoatGloss = clearcoatGloss;

				mo->setMaterial(mat);
			}

			this->objects.emplace_back(mo);
			index++;
		}
	}

#endif	// Will be deprecated: only read gltf models
#if ENABLE_ANIMATION
	loadGLTF("../Assets/phoenix_bird/scene.gltf", vulkanBackend);
#else
	loadGLTF("../Assets/glTF_sample_models/SheenChair/SheenChair/glTF/SheenChair.gltf", vulkanBackend);
#endif
}

void Scene::save(const std::string& path) const {}

void Scene::beginFrame(const float fixedDeltaTime)
{
#if ENABLE_ANIMATION
	for (Animation& anim : _animations)
	{
		anim.update(fixedDeltaTime);
	}

	const uint32 objectCount = objects.size();
	for (uint32 objectIndex = 0; objectIndex < objectCount; ++objectIndex)
	{
		std::unique_ptr<SceneObject>& sc = objects[objectIndex];

		//if (sc->isKindOf<MeshObject>() == false)
		//	continue;

		MeshObject* mo = static_cast<MeshObject*>(sc.get());

		if (mo->getResource()->_jointIndicesData.empty() || mo->getResource()->_weightsData.empty())
		{
			// static mesh
		}
		else
		{
			// skinned mesh
			Skeleton& skeleton = _skeletons[mo->_skeletonIndex];
			Animation& animation = _animations[mo->_animationIndex];

			const uint32 boneCount = skeleton._boneArray.size();

			std::vector<Mat4x4> animationMatrices;
			std::vector<Mat4x4> skinningMatrices;
			animationMatrices.resize(boneCount);
			skinningMatrices.resize(boneCount);

			for (uint32 i = 0; i < boneCount; ++i)
			{
				const Bone& bone = skeleton._boneArray[i];

				Vec3 translation;
				Vec3 scale(1, 1, 1);
				Vec4 rotation;
				animation.sample(fixedDeltaTime, bone._bonaName, translation, rotation, scale);

				auto makeMat3x3 = [&](Vec4& qua)->Mat3x3
					{
						float x = qua.x;
						float y = qua.y;
						float z = qua.z;
						float w = qua.w;

						float xx = x * x;
						float yy = y * y;
						float zz = z * z;
						float xy = x * y;
						float xz = x * z;
						float yz = y * z;
						float wx = w * x;
						float wy = w * y;
						float wz = w * z;

						Mat3x3 mat;
						mat.m00 = 1.0f - 2.0f * (yy + zz); // m00
						mat.m10 = 2.0f * (xy + wz);        // m10
						mat.m20 = 2.0f * (xz - wy);        // m20

						mat.m01 = 2.0f * (xy - wz);        // m01
						mat.m11 = 1.0f - 2.0f * (xx + zz); // m11
						mat.m21 = 2.0f * (yz + wx);        // m21

						mat.m02 = 2.0f * (xz + wy);        // m02
						mat.m12 = 2.0f * (yz - wx);        // m12
						mat.m22 = 1.0f - 2.0f * (xx + yy);// m22

						return mat;
					};
				auto makeMatrix = [&]()->Mat4x4
					{
						Mat3x3 sca{ scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z };
						Mat3x3 rot = makeMat3x3(rotation);

						Mat3x3 rs = rot * sca;

						Mat4x4 mat;
						mat.m00 = rs.m00;
						mat.m01 = rs.m01;
						mat.m02 = rs.m02;
						mat.m03 = translation.x;

						mat.m10 = rot.m10;
						mat.m11 = rot.m11;
						mat.m12 = rot.m12;
						mat.m13 = translation.y;

						mat.m20 = rot.m20;
						mat.m21 = rot.m21;
						mat.m22 = rot.m22;
						mat.m23 = translation.z;

						mat.m30 = 0.0f;
						mat.m31 = 0.0f;
						mat.m32 = 0.0f;
						mat.m33 = 1.0f;

						return mat;
					};

				Mat4x4 animationMatrix = makeMatrix();
				if (bone._parentBoneIndex >= 0)
				{
					const Bone& parentBone = skeleton._boneArray[bone._parentBoneIndex];
					animationMatrix = mul(animationMatrices[bone._parentBoneIndex], animationMatrix);
				}

				animationMatrices[i] = animationMatrix;
			}

			for (uint32 i = 0; i < boneCount; ++i)
			{
				const Bone& bone = skeleton._boneArray[i];
				A3_ASSERT_DEV("LongtimeProdity", skeleton._boneNameIndexMapper.find(bone._bonaName) != skeleton._boneNameIndexMapper.end(), "");
				A3_ASSERT_DEV("LongtimeProdity", skeleton._boneNameIndexMapper[bone._bonaName] == i, "");

#if 0
				const Mat4x4& animationMatrix = skeleton._boneDressPoseArray[i];	//  T-Pose 그리기
#else
				const Mat4x4& animationMatrix = animationMatrices[i];				// animation pose 그리기
#endif
				const Mat4x4& dressPoseInvMatrix = skeleton._boneDressPoseInverseArray[i];
				skinningMatrices[i] = mul(animationMatrix, dressPoseInvMatrix);
			}

			std::vector<VertexPosition> skinnedPositions;
			std::vector<VertexAttributes> skinnedNormals;
			const uint32 vertexCount = mo->getResource()->positions.size();

			skinnedPositions.resize(vertexCount);
			skinnedNormals.resize(vertexCount);

			for (uint32 i = 0; i < vertexCount; ++i)
			{
				Mat4x4 skinningMatrix = Mat4x4::identity;
				skinningMatrix.m00 = 0;
				skinningMatrix.m11 = 0;
				skinningMatrix.m22 = 0;
				skinningMatrix.m33 = 0;

				float sumWeight = 0.0f;
				A3_ASSERT_DEV("LongtimeProdigy", mo->getResource()->_jointIndicesData.size() == mo->getResource()->_weightsData.size(), "");
				for (uint32 j = 0; j < 4; ++j)
				{
					float weight;
					int boneIndex;
					switch (j)
					{
					case 0:
						weight = mo->getResource()->_weightsData[i].x;
						boneIndex = mo->getResource()->_jointIndicesData[i].x;
						break;
					case 1:
						weight = mo->getResource()->_weightsData[i].y;
						boneIndex = mo->getResource()->_jointIndicesData[i].y;
						break;
					case 2:
						weight = mo->getResource()->_weightsData[i].z;
						boneIndex = mo->getResource()->_jointIndicesData[i].z;
						break;
					case 3:
						weight = mo->getResource()->_weightsData[i].w;
						boneIndex = mo->getResource()->_jointIndicesData[i].w;
						break;
					default:
						break;
					}

					skinningMatrix += skinningMatrices[boneIndex] * weight;
					sumWeight += weight;
				}

				A3_ASSERT_DEV("LongtimeProdigy", AreSame(sumWeight, 1.0f, 0.0001f), "Skinning Weight의 합은 반드시 1이어야합니다.");

#if 0			// inverse(transpose사용))
				auto inverseMatrix = [](const Mat3x3& mat, Mat3x3& inv) -> bool
					{
						// Calculate the determinant
						float det = mat[0][0] * (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) -
							mat[0][1] * (mat[1][0] * mat[2][2] - mat[1][2] * mat[2][0]) +
							mat[0][2] * (mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0]);

						// Check if the determinant is non-zero
						if (det == 0) {
							std::cout << "Matrix is not invertible (determinant is 0)." << std::endl;
							return false;
						}

						float invDet = 1.0f / det;

						// Calculate the adjugate matrix and multiply by the inverse of the determinant
						inv[0][0] = (mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2]) * invDet;
						inv[0][1] = (mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2]) * invDet;
						inv[0][2] = (mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1]) * invDet;
						inv[1][0] = (mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2]) * invDet;
						inv[1][1] = (mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0]) * invDet;
						inv[1][2] = (mat[1][0] * mat[0][2] - mat[0][0] * mat[1][2]) * invDet;
						inv[2][0] = (mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1]) * invDet;
						inv[2][1] = (mat[2][0] * mat[0][1] - mat[0][0] * mat[2][1]) * invDet;
						inv[2][2] = (mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1]) * invDet;

						return true;
					};

				Mat3x3 tempSkinningMatrix3x3;
				tempSkinningMatrix3x3.m00 = skinningMatrix.m00;
				tempSkinningMatrix3x3.m01 = skinningMatrix.m01;
				tempSkinningMatrix3x3.m02 = skinningMatrix.m02;

				tempSkinningMatrix3x3.m10 = skinningMatrix.m10;
				tempSkinningMatrix3x3.m11 = skinningMatrix.m11;
				tempSkinningMatrix3x3.m12 = skinningMatrix.m12;

				tempSkinningMatrix3x3.m20 = skinningMatrix.m20;
				tempSkinningMatrix3x3.m21 = skinningMatrix.m21;
				tempSkinningMatrix3x3.m22 = skinningMatrix.m22;

				Mat3x3 skinningMatrixASD;
				const bool testtt = inverseMatrix(tempSkinningMatrix3x3, skinningMatrixASD);
				A3_ASSERT_DEV("LongtimeProdigy", testtt, "");

				Mat4x4 temptempSkinningMatrix;
				temptempSkinningMatrix.m00 = skinningMatrixASD.m00;
				temptempSkinningMatrix.m01 = skinningMatrixASD.m01;
				temptempSkinningMatrix.m02 = skinningMatrixASD.m02;

				temptempSkinningMatrix.m10 = skinningMatrixASD.m10;
				temptempSkinningMatrix.m11 = skinningMatrixASD.m11;
				temptempSkinningMatrix.m12 = skinningMatrixASD.m12;

				temptempSkinningMatrix.m20 = skinningMatrixASD.m20;
				temptempSkinningMatrix.m21 = skinningMatrixASD.m21;
				temptempSkinningMatrix.m22 = skinningMatrixASD.m22;

				temptempSkinningMatrix.m03 = 0;
				temptempSkinningMatrix.m13 = 0;
				temptempSkinningMatrix.m23 = 0;
				temptempSkinningMatrix.m30 = 0;
				temptempSkinningMatrix.m31 = 0;
				temptempSkinningMatrix.m32 = 0;
				temptempSkinningMatrix.m33 = 1;

				temptempSkinningMatrix.transpose();

				temptempSkinningMatrix.m03 = skinningMatrix.m03;
				temptempSkinningMatrix.m13 = skinningMatrix.m13;
				temptempSkinningMatrix.m23 = skinningMatrix.m23;
#endif

				const VertexPosition& position = mo->getResource()->positions[i];
				const VertexAttributes& attr = mo->getResource()->attributes[i];
				const float* normal = attr.normals;

				skinnedPositions[i] = skinningMatrix * position;
				Vec4 skinnedNormal = skinningMatrix * Vec4(normal[0], normal[1], normal[2], 0);

				float len2 = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
				float len = std::sqrt(skinnedNormal.x * skinnedNormal.x + skinnedNormal.y * skinnedNormal.y + skinnedNormal.z * skinnedNormal.z);

				skinnedNormals[i].normals[0] = skinnedNormal.x;
				skinnedNormals[i].normals[1] = skinnedNormal.y;
				skinnedNormals[i].normals[2] = skinnedNormal.z;
				skinnedNormals[i].normals[3] = 0;
				skinnedNormals[i].uvs[0] = attr.uvs[0];
				skinnedNormals[i].uvs[1] = attr.uvs[1];
				skinnedNormals[i].uvs[2] = attr.uvs[2];
				skinnedNormals[i].uvs[3] = attr.uvs[3];
			}

			mo->_skinnedPositions = std::move(skinnedPositions);
			mo->_skinnedAttributes = std::move(skinnedNormals);
		}
	}
#endif // ENABLE_ANIMATION
}

void Scene::endFrame()
{
	//bSceneDirty = false;
}

std::vector<MeshObject*> Scene::collectMeshObjects() const
{
	std::vector<MeshObject*> outObjects;

	for (const std::unique_ptr<SceneObject>& so : objects)
	{
		if (so->canRender())
			outObjects.push_back(static_cast<MeshObject*>(so.get()));
	}

	return outObjects;
}

A3::MaterialParameter::MaterialParameter()
{
	_baseColorFactor = Vec4(1);
	_baseColorTexture = TextureManager::gWhiteParameter;
	_normalTexture = TextureManager::gWhiteParameter;
	_occlusionTexture = TextureManager::gWhiteParameter;

	_metallicFactor = 0;
	_roughnessFactor = 1;
	_metallicRoughnessTexture = TextureManager::gWhiteParameter;
	_transmissionFactor = 0.0f;
    _transmissionTexture = TextureManager::gWhiteParameter;
    _ior = 1.5f;

	_emissiveFactor = Vec3(0);
	_emissiveTexture = TextureManager::gWhiteParameter;
	
	// ======== using sheen material ========
	// ======================================
	_sheenColorFactor = Vec3(0);
	_sheenRoughnessFactor = 0;
	_sheenColorTexture = TextureManager::gWhiteParameter;
	_sheenRoughnessTexture = TextureManager::gWhiteParameter;
	// ======================================

	//// ================== Disney BRDF 호환 ===============================
	//// Subsurface (대응: KHR_materials_volume.thicknessFactor)
	//float _subsurfaceFactor = 0.0f;
	//TextureParameter _subsurfaceTexture = TextureManager::gWhiteParameter;

	//// Specular & SpecularTint (대응: KHR_materials_specular)
	//float _specularFactor = 0.5f;          // Disney default=0.5
	//Vec3  _specularColorFactor = Vec3(1.0);     // tint용
	//TextureParameter _specularTexture = TextureManager::gWhiteParameter;
	//TextureParameter _specularColorTexture = TextureManager::gWhiteParameter;

	//// Anisotropy (대응: KHR_materials_anisotropy)
	//float _anisotropicFactor = 0.0f;
	//Vec2  _anisotropyDirection = Vec2(1.0, 0.0);
	//TextureParameter _anisotropyTexture = TextureManager::gWhiteParameter;

	//// Clearcoat (대응: KHR_materials_clearcoat)
	//float _clearcoatFactor = 0.0f;
	//float _clearcoatRoughnessFactor = 0.0f;
	//TextureParameter _clearcoatTexture = TextureManager::gWhiteParameter;
	//TextureParameter _clearcoatRoughTex = TextureManager::gWhiteParameter;
	//TextureParameter _clearcoatNormalTex = TextureManager::gWhiteParameter;
	//// =================================================================
}
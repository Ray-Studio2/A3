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
			local = mul(local, quaternionToMatrix(quat));

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

	std::vector<Skeleton> skeletonArr;

	struct AnimationData
	{

	};
	std::vector<AnimationData> animationArr;

	skeletonArr.reserve(model.skins.size());
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

		std::unordered_map<int, size_t> nodeIndexToBoneIndexMap;
		for (size_t i = 0; i < skin.joints.size(); ++i) {
			int jointIndex = skin.joints[i];
			if (jointIndex >= 0 && jointIndex < model.nodes.size()) {
				const auto& jointNode = model.nodes[jointIndex];
				std::string boneName = jointNode.name.empty() ? ("joint_" + std::to_string(jointIndex)) : jointNode.name;
				Mat4x4 bindPoseLocalMatrix = getNodeLocalTransform(jointNode);

				Bone bone;
				bone._bonaName = boneName;
				bone._childBoneIndexArr = jointNode.children;

				skeleton._boneArray.push_back(std::move(bone));

				nodeIndexToBoneIndexMap[jointIndex] = skeleton._boneArray.size() - 1;

				skeleton._boneDressPoseArray.push_back(bindPoseLocalMatrix);
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
				skeleton._boneArray[childBoneIndex]._parentBoneIndex = i;
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

		skeletonArr.push_back(std::move(skeleton));
	}

	//std::cout << "\n--- Animations ---" << std::endl;
	//if (!model.animations.empty()) {
	//	const auto& anim = model.animations[0];
	//	std::cout << "\n--- Animation: " << (anim.name.empty() ? "(unnamed)" : anim.name) << " ---" << std::endl;

	//	AnimationData animData;

	//	for (const auto& channel : anim.channels) 
	//	{
	//		std::string boneName = model.nodes[channel.target_node].name.empty() ? ("joint_" + std::to_string(channel.target_node)) : model.nodes[channel.target_node].name;
	//		AnimationData animData;

	//		std::string targetPath = channel.target_path;
	//		int samplerIndex = channel.sampler;

	//		if (samplerIndex >= 0 && samplerIndex < anim.samplers.size()) 
	//		{
	//			const auto& sampler = anim.samplers[samplerIndex];
	//			if (sampler.input >= 0 && sampler.input < model.accessors.size() && sampler.output >= 0 && sampler.output < model.accessors.size()) 
	//			{
	//				const auto& timeAccessor = model.accessors[sampler.input];
	//				const auto& valueAccessor = model.accessors[sampler.output];

	//				std::vector<float> times;
	//				if (timeAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && timeAccessor.type == TINYGLTF_TYPE_SCALAR) 
	//				{
	//					times = getBufferData<float>(model, sampler.input);
	//				}

	//				//if (valueAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
	//				//	if (valueAccessor.type == TINYGLTF_TYPE_VEC3 && targetPath == "translation") {
	//				//		animData._translation = getBufferData<Vec3>(model, sampler.output);
	//				//		animData._times = times;
	//				//	}
	//				//	else if (valueAccessor.type == TINYGLTF_TYPE_VEC4 && targetPath == "rotation") {
	//				//		animData._rotation = getBufferData<Vec4>(model, sampler.output);
	//				//		if (animData._times.empty()) animData._times = times;
	//				//	}
	//				//	else if (valueAccessor.type == TINYGLTF_TYPE_VEC3 && targetPath == "scale") {
	//				//		animData._scale = getBufferData<Vec3>(model, sampler.output);
	//				//		if (animData._times.empty()) animData._times = times;
	//				//	}
	//				//}
	//			}
	//		}
	//	}
	//	//if (!animData._times.empty()) {
	//	//	_animations.push_back(animData);
	//	//}
	//}

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

		//// 6. Alpha Mode
		//std::cout << "  Alpha Mode: " << material.alphaMode << std::endl;
		//if (material.alphaMode == "MASK") 
		//{
		//	std::cout << "  Alpha Cutoff: " << material.alphaCutoff << std::endl;
		//}

		//// 7. Double Sided
		//std::cout << "  Double Sided: " << (material.doubleSided ? "True" : "False") << std::endl;

		Material a3Material;
		a3Material._parameter = std::move(materialParameter);
		a3Material._buffer = vulkanBackend.createResourceBuffer(sizeof(a3Material._parameter), static_cast<const void*>(&a3Material._parameter));
		a3Material._name = material.name;
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
							/*if (componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
								std::vector<Vec4> data = getBufferData<Vec4>(model, accessorIndex);
								std::vector<Vec4> converted(data.size());
								for (size_t i = 0; i < data.size(); ++i) {
									converted[i] = Vec4(data[i].x, data[i].y, data[i].z, data[i].w);
								}
								return converted;
							}
							else */if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
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

			std::vector<Vec3> positions = getVec3Attribute("POSITION");
			std::vector<Vec3> normals = getVec3Attribute("NORMAL");
			std::vector<Vec2> uvs = getVec2Attribute("TEXCOORD_0");
			std::vector<Vec4> jointIndicesData = getVec4Attribute("JOINTS_0", TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
			std::vector<Vec4> weightsData = getVec4Attribute("WEIGHTS_0", TINYGLTF_COMPONENT_TYPE_FLOAT);

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
				vPositions.push_back(VertexPosition(positions[i].x * 0.02f, positions[i].y * 0.02f, positions[i].z * 0.02f, 1.0f));
				vAttributes.push_back(VertexAttributes({ normals[i].x, normals[i].y, normals[i].z, 0.0f }, { uvs[i].x, uvs[i].y, 0.0f, 0.0f }));
			}

			if (resources.find(mesh.name) == resources.end()) {
				MeshResource resource;
				resource.positions = vPositions;
				resource.attributes = vAttributes;
				resource.indices = indices;
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

			MeshObject* mo = new MeshObject(resources[mesh.name], &materialArr[primitive.material]);
			//mo->setPosition(Vec3(position[0], position[1], position[2]));
			//mo->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
			//mo->setScale(Vec3(scale[0], scale[1], scale[2]));

			mo->setBaseColor(Vec3(1.0f, 1.0f, 1.0f));
			//if (materialName == "light") {
			//	lightIndex.push_back(index);
			//	auto& emittance = material["emittance"];
			//	mo->setEmittance(emittance);
			//}
			//else {
			//	mo->setMetallic(metallic.get<float>());
			//	mo->setRoughness(roughness.get<float>());
			//}
			mo->setMetallic(0.0f);
			mo->setRoughness(1.0f);
			mo->setName(mesh.name);

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
			auto& roughness = material["roughness"];

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

			Material* a3Material = new Material();	///////////////// Juhwan

			a3Material->_parameter = materialParameter;
			a3Material->_buffer = vulkanBackend.createResourceBuffer(sizeof(a3Material->_parameter), static_cast<const void*>(&a3Material->_parameter));
			a3Material->_name = materialName;

			
			//if (material["emittance"] != nullptr)
			//	a3Material->_emittanceFactor = material["emittance"];	///////////////// Juhwan
			//else
			//	a3Material->_emittanceFactor = 1;
				

			//bool isContained = false;
			//for (auto i : materialArrForObj)
			//	if (i._name.compare(a3Material->_name) == 0)	isContained = true;	///////////////// Juhwan

			//if(!isContained)
				materialArrForObj.push_back(*a3Material);	///////////////// Juhwan

			MeshObject* mo = new MeshObject(resources[mesh], a3Material);// &materialArrForObj[materialArrForObj.size() - 1]);
			mo->setName(name);													///////////////// Juhwan
			mo->setPosition(Vec3(position[0], position[1], position[2]));
			mo->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
			mo->setScale(Vec3(scale[0], scale[1], scale[2]));
			mo->calculateTriangleArea();

			mo->setBaseColor(Vec3(baseColor[0], baseColor[1], baseColor[2]));
			if (materialName == "light") {
				lightIndex.push_back(index);
				auto& emittance = material["emittance"];
				mo->setEmittance(emittance);
			}
			else {
				mo->setMetallic(metallic.get<float>());
				mo->setRoughness(roughness.get<float>());
			}

			this->objects.emplace_back(mo);
			index++;
		}
	}
#if 0
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	std::string filePath = "C:/Users/dhfla/Desktop/Projects/Universe/asset/phoenix_bird/scene.gltf";

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);

	if (!warn.empty()) {
		throw std::runtime_error("Warning: " + warn);
	}

	if (!err.empty()) {
		throw std::runtime_error("Error: " + err);
	}

	if (!ret) {
		throw std::runtime_error("Fail to parsing glTF\nPath: " + filePath);
	}

	std::cout << "Success to load glTF!" << std::endl;

	const tinygltf::Scene& scene = model.scenes[model.defaultScene];

	for (size_t i = 0; i < scene.nodes.size(); ++i) {
		const tinygltf::Node& node = model.nodes[scene.nodes[i]];
		std::cout << "Node Name: " << node.name << std::endl;

		if (node.mesh > -1) {
			const tinygltf::Mesh& mesh = model.meshes[node.mesh];
			std::cout << "  - Mesh Name: " << mesh.name << std::endl;

			for (size_t i = 0; i < mesh.primitives.size(); ++i) {
				const tinygltf::Primitive& primitive = mesh.primitives[i];

				// primitive.attributes is contain attributename and accessor index
				for (auto const& [name, accessor_idx] : primitive.attributes) {
					std::cout << "    - Attribute: " << name << std::endl;

					const tinygltf::Accessor& accessor = model.accessors[accessor_idx];
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

					const uint32 vertexCount = accessor.count;

					if (name == "POSITION") {
						const unsigned char* data_start = &buffer.data[bufferView.byteOffset + accessor.byteOffset];
						const float* positions = reinterpret_cast<const float*>(data_start);

						for (size_t v = 0; v < vertexCount; ++v) {
							float x = positions[v * 3 + 0];
							float y = positions[v * 3 + 1];
							float z = positions[v * 3 + 2];
						}
					}
				}

				if (primitive.indices > -1) {
					const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
					const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
					const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

					// 인덱스 데이터 시작 위치
					const unsigned char* data_start = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

					// 인덱스 타입(unsigned short, unsigned int 등)에 따라 적절히 캐스팅해야 합니다.
					// indexAccessor.componentType을 확인하여 처리합니다.
				}
			}
		}
	}
#else
	loadGLTF("../Assets/phoenix_bird/scene.gltf", vulkanBackend);
#endif
}

void Scene::save(const std::string& path) const {}

void Scene::beginFrame()
{

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

	_emissiveFactor = Vec3(0);
	_emissiveTexture = TextureManager::gWhiteParameter;
}

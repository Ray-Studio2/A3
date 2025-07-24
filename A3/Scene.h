#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "EngineTypes.h"
#include "Vector.h"

namespace A3
{
class SceneObject;
class MeshObject;
class CameraObject;
struct MeshResource;
class VulkanRenderBackend;
struct Material;
struct Skeleton;

struct imguiParam // TODO: right for being part of scene?
{
	uint32 maxDepth = 5;
	uint32 numSamples = 1;
	uint32 isProgressive = 1;
	float envmapRotDeg = 0.0f; // ???깃꼍???嶺?GPU?????삳낵繞?
	// TODO: separate CPU side and GPU side

	Vec3 lightPos = Vec3(0.0f);
	uint32 frameCount = 128;
	
	enum LightSamplingMode : uint32 {
		BruteForce = 0,
		NEE
	};
	enum LightSelection : uint32 {
		LightOnly = 0,
		EnvMap,
		Both
	};

	uint32 lightSamplingMode = BruteForce;
	uint32 lightSelection = LightOnly;
};

enum class SceneDirty : uint8 {
	None		= 0,
	Geometry	= 1 << 0,	// mesh ?怨뺣뼺?, ????
	Transform	= 1 << 1,	// ?熬곣뫚?? ????? ??????곌떠???
	GpuBuffer	= 1 << 2,
};

template<typename T>
struct AnimationNode
{
	std::vector<T> _data;
	std::vector<float> _time;
};
struct AnimationData
{
	AnimationNode<Vec3> _translation;
	AnimationNode<Vec4> _rotation;
	AnimationNode<Vec3> _scale;
};
struct Animation
{
	std::unordered_map<std::string, AnimationData> _animData;
	float _totalTime = 0.0f;
	float _currentTime = 0.0f;

	void sample(const float deltaTime, const std::string& boneName, Vec3& outTranslation, Vec4& outRotation, Vec3& outScale)
	{
		_currentTime += deltaTime;
		if (_currentTime >= _totalTime)
			_currentTime -= _totalTime;

		const auto& iter = _animData.find(boneName);
		if (iter == _animData.end())
			return;

		const AnimationData& animData = iter->second;

		Vec3 translation, scale;
		Vec4 rotation;

		// binary search揶쎛 ?源낅뮟 ?ル뿭?筌?. 域뮤筌????椰?筌≪뼚??.
		const uint32 translationCount = animData._translation._time.size();
		for (uint32 i = 0; i < translationCount; ++i)
		{
			if (_currentTime > animData._translation._time[i] && i < translationCount - 1) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._translation._time[i - 1] : 0.0f;
			const float time = animData._translation._time[i];
			const float lerpValue = std::clamp((time - _currentTime) / (time - prevTime), 0.0f, 1.0f);

			static const Vec3& kTemp = Vec3(0, 0, 0);
			const Vec3& prev = i != 0 ? animData._translation._data[i - 1] : kTemp;
			const Vec3& curr = animData._translation._data[i];

			translation = prev * (1 - lerpValue) + curr * lerpValue;
			break;
		}

		const uint32 rotationCount = animData._rotation._time.size();
		for (uint32 i = 0; i < rotationCount; ++i)
		{
			if (_currentTime > animData._rotation._time[i] && i < rotationCount - 1) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._rotation._time[i - 1] : 0.0f;
			const float time = animData._rotation._time[i];
			const float lerpValue = std::clamp((_currentTime - time) / (time - prevTime), 0.0f, 1.0f);

			static const Vec4& kTemp = Vec4(0, 0, 0, 0);
			const Vec4& prev = i != 0 ? animData._rotation._data[i - 1] : kTemp;
			const Vec4& curr = animData._rotation._data[i];

			rotation = prev * (1 - lerpValue) + curr * lerpValue;
			break;
		}

		const uint32 scaleCount = animData._scale._time.size();
		for (uint32 i = 0; i < scaleCount; ++i)
		{
			if (_currentTime > animData._scale._time[i] && i < scaleCount - 1) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._scale._time[i - 1] : 0.0f;
			const float time = animData._scale._time[i];
			const float lerpValue = std::clamp((_currentTime - time) / (time - prevTime), 0.0f, 1.0f);

			static const Vec3& kTemp = Vec3(0, 0, 0);
			const Vec3& prev = i != 0 ? animData._scale._data[i - 1] : kTemp;
			const Vec3& curr = animData._scale._data[i];

			scale = prev * (1 - lerpValue) + curr * lerpValue;
			break;
		}

		outTranslation = translation;
		outRotation = rotation;
		outScale = scale;
	}
};

class Scene
{
public:
	Scene();
	~Scene();

public:
	void load(const std::string &path, VulkanRenderBackend& vulkanBackend);
    void save(const std::string &path) const;

	void beginFrame(const float fixedDeltaTime);
	void endFrame();

	std::vector<MeshObject*> collectMeshObjects() const;
	CameraObject* getCamera() const { return camera.get(); }
	imguiParam* getImguiParam() const { return imgui_param.get(); }
	const std::vector<uint32>& getLightIndex() const { return lightIndex; }

	void markSceneDirty() { bSceneDirty = true; bPosUpdated = true; bBufferUpdated = true; }
	void cleanSceneDirty() { bSceneDirty = false; }
	bool isSceneDirty() const { return bSceneDirty; }

	void markBufferUpdated() { bBufferUpdated = true; }
	void cleanBufferUpdated() { bBufferUpdated = false; }
	bool isBufferUpdated() const { return bBufferUpdated; }

	void markPosUpdated() { bPosUpdated = true; bBufferUpdated = true; }
	void cleanPosUpdated() { bPosUpdated = false; }
	bool isPosUpdated() const { return bPosUpdated; }

private:
	void loadGLTF(const std::string& fileName, VulkanRenderBackend& vulkanBackend);

private:
	bool bSceneDirty;
	bool bBufferUpdated;
	bool bPosUpdated;

	std::vector<Skeleton> _skeletons;

	std::vector<Animation> _animations;

    std::unordered_map<std::string, MeshResource*> resources;
    std::vector<std::unique_ptr<SceneObject>> objects;
	std::unique_ptr<CameraObject> camera;
	std::unique_ptr<imguiParam> imgui_param;

	std::vector<Material> materialArrForObj;
	std::vector<Material> materialArr;

	std::vector<uint32> lightIndex;
};
}

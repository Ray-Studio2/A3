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
	uint32 isProgressive = 0;
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

	void update(const float deltaTime)
	{
		_currentTime += deltaTime;
		if (_currentTime >= _totalTime)
			_currentTime -= _totalTime;

		//_currentTime = 0.24f;
	}

	void sample(const float deltaTime, const std::string& boneName, Vec3& outTranslation, Vec4& outRotation, Vec3& outScale)
	{
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
			if (_currentTime > animData._translation._time[i]) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._translation._time[i - 1] : 0.0f;
			const float time = animData._translation._time[i];
			const float lerpValue = std::clamp((_currentTime - prevTime) / (time - prevTime), 0.0f, 1.0f);

			static const Vec3& kTemp = Vec3(0, 0, 0);
			const Vec3& prev = i != 0 ? animData._translation._data[i - 1] : kTemp;
			const Vec3& curr = animData._translation._data[i];

			translation = prev * (1 - lerpValue) + curr * lerpValue;
			break;
		}

		const uint32 rotationCount = animData._rotation._time.size();
		for (uint32 i = 0; i < rotationCount; ++i)
		{
			if (_currentTime > animData._rotation._time[i]) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._rotation._time[i - 1] : 0.0f;
			const float time = animData._rotation._time[i];
			const float lerpValue = std::clamp((_currentTime - prevTime) / (time - prevTime), 0.0f, 1.0f);

			static const Vec4& kTemp = Vec4(0, 0, 0, 0);
			const Vec4& prev = i != 0 ? animData._rotation._data[i - 1] : kTemp;
			const Vec4& curr = animData._rotation._data[i];

			// 쿼터니언 정규화
			auto Normalize = [](const Vec4& qua) -> Vec4 {
				float magSq = qua.w * qua.w + qua.x * qua.x + qua.y * qua.y + qua.z * qua.z;
				if (magSq > 0.0f) {
					float invMag = 1.0f / std::sqrt(magSq);
					return Vec4(qua.x * invMag, qua.y * invMag, qua.z * invMag, qua.w * invMag);
				}
				return Vec4(qua.x, qua.y, qua.z, qua.w); // 단위 쿼터니언 반환 (0 벡터는 정규화 불가)
				};
			auto Slerp = [&](const Vec4& q1, const Vec4& q2, float t)->Vec4 {
				// 1. 쿼터니언 정규화 (입력 쿼터니언이 이미 정규화되어 있다고 가정해도 안전을 위해 다시 합니다.)
				Vec4 startQ = Normalize(q1);
				Vec4 endQ = Normalize(q2);

				// 2. 두 쿼터니언의 내적 계산 (코사인 값)
				float dot = startQ.x * endQ.x + startQ.y * endQ.y + startQ.z * endQ.z + startQ.w * endQ.w;

				// 3. 내적값이 음수이면 한 쿼터니언의 부호를 반전시켜 가장 짧은 경로를 따르도록 합니다.
				//    (q와 -q는 같은 회전을 나타내지만 다른 경로를 가집니다.)
				if (dot < 0.0f) {
					endQ = endQ * -1.0f; // 부호 반전
					dot = -dot;          // 내적값도 반전
				}

				// 4. 두 쿼터니언이 매우 가까우면 선형 보간(Lerp)으로 대체합니다.
				//    이는 작은 각도에서 나눗셈 오류를 피하고 성능을 향상시킵니다.
				const float DOT_THRESHOLD = 0.9995f; // 임계값, 필요에 따라 조정 가능
				if (dot > DOT_THRESHOLD) {
					// 선형 보간 (Lerp)
					// Lerp는 정규화되지 않은 쿼터니언을 반환하므로 마지막에 정규화해야 합니다.
					Vec4 result = startQ + ((endQ - startQ) * t); // (q2 - q1) * t + q1
					return Normalize(result);
				}

				float theta_0 = std::acos(dot);        // angle between input quaternions
				float theta = theta_0 * t;             // angle between q1 and result
				float sin_theta = std::sin(theta);     // compute this value only once
				float sin_theta_0 = std::sin(theta_0); // compute this value only once

				float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;  // == sin(theta_0 - theta) / sin(theta_0)
				float s1 = sin_theta / sin_theta_0;
				
				return (startQ * s0) + (endQ * s1);
				};

			rotation = Slerp(prev, curr, lerpValue);
			//rotation = prev * (1 - lerpValue) + curr * lerpValue;
			break;
		}

		const uint32 scaleCount = animData._scale._time.size();
		for (uint32 i = 0; i < scaleCount; ++i)
		{
			if (_currentTime > animData._scale._time[i]) // 筌띾뜆?筌??紐껊굡繹먮슣? 筌≪뼚? 筌륁궢釉?쭖?域밸챶源?筌띾뜆?筌??紐껊굡 ??源?
				continue;

			const float prevTime = i != 0 ? animData._scale._time[i - 1] : 0.0f;
			const float time = animData._scale._time[i];
			const float lerpValue = std::clamp((_currentTime - prevTime) / (time - prevTime), 0.0f, 1.0f);

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

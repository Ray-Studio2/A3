#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "EngineTypes.h"
#include "Vector.h"

namespace A3
{
class SceneObject;
class MeshObject;
class CameraObject;
struct MeshResource;

struct imguiParam // TODO: right for being part of scene?
{
	uint32 maxDepth = 5;
	uint32 numSamples = 1;
	uint32 isProgressive = 1;
	float envmapRotDeg = 0.0f; // 여기까지만 GPU에 넘겨줌
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
	Geometry	= 1 << 0,	// mesh 추가, 삭제
	Transform	= 1 << 1,	// 위치, 회전, 스케일 변경
	GpuBuffer	= 1 << 2,
};

class Scene
{
public:
	Scene();
	~Scene();

public:
	void load(const std::string &path);
    void save(const std::string &path) const;

	void beginFrame();
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
	void loadGLTF(const std::string& fileName);

private:
	bool bSceneDirty;
	bool bBufferUpdated;
	bool bPosUpdated;

    std::unordered_map<std::string, MeshResource*> resources;
    std::vector<std::unique_ptr<SceneObject>> objects;
	std::unique_ptr<CameraObject> camera;
	std::unique_ptr<imguiParam> imgui_param;

	std::vector<uint32> lightIndex;
};
}

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "EngineTypes.h"

namespace A3
{
class SceneObject;
class MeshObject;
class CameraObject;
struct MeshResource;

struct imguiParam
{
	uint32 maxDepth = 2;
	uint32 numSamples = 128;
	uint32 isProgressive = 1;

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

	void markSceneDirty() { bSceneDirty = true; }
	void cleanSceneDirty() { bSceneDirty = false; }
	bool isSceneDirty() const { return bSceneDirty; }

private:
	bool bSceneDirty;

    std::unordered_map<std::string, MeshResource*> resources;
    std::vector<std::unique_ptr<SceneObject>> objects;
	std::unique_ptr<CameraObject> camera;
	std::unique_ptr<imguiParam> imgui_param;
};
}

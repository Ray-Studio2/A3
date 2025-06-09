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

struct SampleQuality 
{
	uint32 maxDepth = 3;
	uint32 numSamples = 64;
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
	SampleQuality* getSampleQuality() const { return sampleQuality.get(); }

	void markSceneDirty() { bSceneDirty = true; }
	void cleanSceneDirty() { bSceneDirty = false; }
	bool isSceneDirty() const { return bSceneDirty; }

private:
	bool bSceneDirty;

    std::unordered_map<std::string, MeshResource*> resources;
    std::vector<std::unique_ptr<SceneObject>> objects;
	std::unique_ptr<CameraObject> camera;
	std::unique_ptr<SampleQuality> sampleQuality;
};
}

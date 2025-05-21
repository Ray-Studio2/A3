#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace A3
{
class SceneObject;
class MeshObject;
class CameraObject;
struct MeshResource;


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

	void markSceneDirty() { bSceneDirty = true; }
	bool isSceneDirty() const { return bSceneDirty; }

private:
	bool bSceneDirty;

    std::unordered_map<std::string, MeshResource*> resources;
    std::vector<std::unique_ptr<SceneObject>> objects;
	std::unique_ptr<CameraObject> camera;
};
}

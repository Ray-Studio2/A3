#include "Scene.h"

#include <fstream>

#include "Utility.h"
#include "MeshObject.h"
#include "MeshResource.h"
#include "CameraObject.h"

#include "Json.hpp"

using namespace A3;
using Json = nlohmann::json;

Scene::Scene()
	: bSceneDirty( true )
{
	this->imgui_param = std::make_unique<imguiParam>();
}

Scene::~Scene() {
    for (auto &resource : resources) {
        delete resource.second;
    }
}

void Scene::load(const std::string &path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!: " + path);
	}

	Json data = Json::parse(file);

	// TODO: camera
	auto &camera = data["camera"];
    if (camera.is_object()) {
        auto &position = camera["position"];
        auto &rotation = camera["rotation"];
        auto &fov = camera["yFovDeg"];
		//auto& resolution = camera["resolution"];
		//auto& sampling = camera["sampling"];
		//auto& maxDepth = camera["maxDepth"];
		//auto& spp = camera["spp"];
		//auto& exposure = camera["exposure"];

		this->camera = std::make_unique<CameraObject>();
		this->camera->setPosition(Vec3(position[0], position[1], position[2]));
		this->camera->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
		this->camera->setFov(fov);
    }

	//auto& materials = data["materials"];

	auto &objects = data["scene"];
	if (objects.is_array()) {
		for (int i = 0; i < objects.size(); ++i)
		{
			auto &object = objects[i];
			auto &position = object["position"];
			auto &rotation = object["rotation"];
			auto &scale = object["scale"];
			auto &mesh = object["mesh"];
			auto &material = object["material"];
			//auto& material = materials[object["material"]];
			//auto& baseColor = material["baseColor"];

			if (resources.find(mesh) == resources.end()) {
				MeshResource resource;
				Utility::loadMeshFile(resource, "../Assets/" + ((std::string)mesh) + ".obj");
				resources[mesh] = new MeshResource(resource);
			}

			MeshObject *mo = new MeshObject(resources[mesh]);
			mo->setPosition(Vec3(position[0], position[1], position[2]));
			mo->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
			mo->setScale(Vec3(scale[0], scale[1], scale[2]));

			//mo->setBaseColor(Vec3(baseColor[0], baseColor[1], baseColor[2]));

			if (material == "light") {
			//	auto& emittance = material["emittance"];
			//	mo->setEmittance(Vec3(emittance[0], emittance[1], emittance[2]));
				lightIndex.push_back(i);
				mo->setEmittance(Vec3(100.0f));
			}

			this->objects.emplace_back(mo);
		}
	}
}

void Scene::save(const std::string &path) const {}

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

	for( const std::unique_ptr<SceneObject>& so : objects )
	{
		if( so->canRender() )
			outObjects.push_back( static_cast< MeshObject* >( so.get() ) );
	}

	return outObjects;
}
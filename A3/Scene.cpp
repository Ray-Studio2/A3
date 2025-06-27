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
	: bSceneDirty(true)
{
	this->imgui_param = std::make_unique<imguiParam>();
}

Scene::~Scene() {
	for (auto& resource : resources) {
		delete resource.second;
	}
}

void Scene::load(const std::string& path) {
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
		auto& resolution = camera["resolution"]; // TODO
		auto& sampling = camera["sampling"];
		auto& maxDepth = camera["maxDepth"];
		auto& spp = camera["spp"];
		auto& exposure = camera["exposure"]; // TODO

		this->camera = std::make_unique<CameraObject>();
		this->camera->setPosition(Vec3(position[0], position[1], position[2]));
		this->camera->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
		this->camera->setFov(fov);

		this->imgui_param->frameCount = spp; // one sampling per frame
		this->imgui_param->maxDepth = maxDepth;
		this->imgui_param->lightSamplingMode = (sampling == "bruteforce" ? imguiParam::BruteForce : imguiParam::NEE);
	}

	//auto& envMap = data["envmap"];

	auto& materials = data["materials"];

	auto& objects = data["sceneComponets"];
	if (objects.is_object()) {
		int index = 0;
		for (auto& [name, object] : objects.items())
		{
			auto& position = object["position"];
			auto& rotation = object["rotation"];
			auto& scale = object["scale"];
			auto& mesh = object["mesh"];
			std::string materialName = object["material"].get<std::string>();
			auto& material = materials[materialName];
			auto& baseColor = material["baseColor"];

			if (resources.find(mesh) == resources.end()) {
				MeshResource resource;
				Utility::loadMeshFile(resource, "../Assets/" + ((std::string)mesh));
				resources[mesh] = new MeshResource(resource);
			}

			MeshObject* mo = new MeshObject(resources[mesh]);
			mo->setPosition(Vec3(position[0], position[1], position[2]));
			mo->setRotation(Vec3(rotation[0], rotation[1], rotation[2]));
			mo->setScale(Vec3(scale[0], scale[1], scale[2]));
			mo->calculateTriangleArea();

			mo->setBaseColor(Vec3(baseColor[0], baseColor[1], baseColor[2]));
			if (materialName == "light") {
				lightIndex.push_back(index);
				auto& emittance = material["emittance"];
				mo->setEmittance(Vec3(emittance, emittance, emittance)); // TODO: divide into light color and emittance
			}

			this->objects.emplace_back(mo);
			index++;
		}
	}
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
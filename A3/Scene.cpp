#include "Scene.h"

#include <fstream>

#include "Utility.h"
#include "MeshObject.h"
#include "MeshResource.h"
#include "CameraObject.h"
#include "RenderSettings.h"

#include "Json.hpp"

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

void Scene::load(const std::string& path) {
	this->resources.clear();
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

			MeshObject* mo = new MeshObject(resources[mesh]);
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
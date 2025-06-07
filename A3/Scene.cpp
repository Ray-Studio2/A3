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
	// static MeshResource resource;
	// Utility::loadMeshFile( resource, "bunny.obj" );
	// MeshObject* mo0 = new MeshObject( &resource );
	// MeshObject* mo1 = new MeshObject( &resource );
	// mo0->setPosition( Vec3( -2, 0, 0 ) );
	// mo1->setPosition( Vec3( 2, 0, 0 ) );
	// objects.emplace_back( mo0 );
	// objects.emplace_back( mo1 );
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

		this->camera = std::make_unique<CameraObject>();
		this->camera->setPosition(Vec3(position[0], position[1], position[2]));
		this->camera->setFov(fov);
    }

	auto &objects = data["objects"];
	if (objects.is_array()) {
		for (auto &object : objects) {
			// auto &type = object["type"];
			auto &name = object["name"];
			auto &mesh = object["mesh"];
			auto &scale = object["scale"];
			auto &position = object["position"];
			auto &rotation = object["rotation"];
			auto &material = object["material"];

			if (resources.find(mesh) == resources.end()) {
				MeshResource resource;
				Utility::loadMeshFile(resource, "../Assets/" + ((std::string)mesh) + ".obj");
				resources[mesh] = new MeshResource(resource);
			}

			MeshObject *mo = new MeshObject(resources[mesh]);
			mo->setPosition(Vec3(position[0], position[1], position[2]));
			mo->setScale(Vec3(scale[0], scale[1], scale[2]));
			this->objects.emplace_back(mo);
			// } else {
			//     throw std::runtime_error("unknown object type: " + type.get<std::string>());
			// }
		}
	}
}

void Scene::save(const std::string &path) const {}

void Scene::beginFrame()
{

}

void Scene::endFrame()
{
	bSceneDirty = false;
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
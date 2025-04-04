#include "Scene.h"
#include "Utility.h"
#include "MeshObject.h"
#include "MeshResource.h"

using namespace A3;

Scene::Scene()
	: bSceneDirty( true )
{
	static MeshResource resource;
	Utility::loadMeshFile( resource, "teapot.obj" );
	MeshObject* mo0 = new MeshObject( &resource );
	MeshObject* mo1 = new MeshObject( &resource );
	mo0->setPosition( Vec3( -2, 0, 0 ) );
	mo1->setPosition( Vec3( 2, 0, 0 ) );
	objects.emplace_back( mo0 );
	objects.emplace_back( mo1 );
}

Scene::~Scene() {}

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
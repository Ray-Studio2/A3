#pragma once

#include "SceneObject.h"
#include "Vulkan.h"
#include "RenderResource.h"
#include "AccelerationStructure.h"
#include <memory>

namespace A3
{
struct MeshResource;

class MeshObject : public SceneObject
{
public:
	MeshObject( MeshResource* inResource )
		: resource( inResource )
	{}

	void createRenderResources( IRenderBackend* backend )
	{
		blasBatch.blas = backend->createBLAS( resource->positions, resource->attributes, resource->indices, Mat3x4::identity );
		blasBatch.transforms = { localToWorld };
	}

	BLASBatch* getBLASBatch() { return &blasBatch; }

	MeshResource* getResource() { return resource; }
	
	Vec3 getWorldPosition() const { return Vec3( localToWorld.m03, localToWorld.m13, localToWorld.m23 ); }

	virtual bool canRender() override { return true; }

private:
	MeshResource* resource;

	BLASBatch blasBatch;
};
}
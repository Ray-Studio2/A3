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
		BLASBuildParams params = {
			.positionData = resource->positions,
			.attributeData = resource->attributes,
			.indexData = resource->indices,
			.cumulativeTriangleAreaData = resource->cumulativeTriangleArea,
			.triangleCount = resource->triangleCount,
			.transformData = Mat3x4::identity,
		};

		blasBatch.blas = backend->createBLAS( params );
		blasBatch.transforms = { localToWorld };
	}

	BLASBatch* getBLASBatch() { return &blasBatch; }

	MeshResource* getResource() { return resource; }

	virtual bool canRender() override { return true; }

private:
	MeshResource* resource;

	BLASBatch blasBatch;
};
}
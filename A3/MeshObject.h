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
			.transformData = Mat3x4::identity
		};
		blasBatch.blas = backend->createBLAS( params );
		blasBatch.transforms = { localToWorld };
	}

	BLASBatch* getBLASBatch() { return &blasBatch; }
	MeshResource* getResource() { return resource; }
	virtual bool canRender() override { return true; }

	void calculateTriangleArea()
	{
		updateLocalToWorld();
		
		uint32 sumIdx = 1;
		for (uint32 triIndex = 0; triIndex < resource->triangleCount; ++triIndex)
		{
			const auto p1 = localToWorld * resource->positions[resource->indices[triIndex * 3 + 0]];
			const auto p2 = localToWorld * resource->positions[resource->indices[triIndex * 3 + 1]];
			const auto p3 = localToWorld * resource->positions[resource->indices[triIndex * 3 + 2]];

			const auto v2_1 = p2 - p1;
			const auto v3_1 = p3 - p1;
			const auto normal = cross(v2_1, v3_1);
			const float magnitude = 0.5f * std::sqrt(dot(normal, normal));

			resource->cumulativeTriangleArea[sumIdx++] = resource->cumulativeTriangleArea[sumIdx - 1] + magnitude;
		}
	}

private:
	MeshResource* resource;

	BLASBatch blasBatch;
};
}
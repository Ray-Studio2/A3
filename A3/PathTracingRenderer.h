#pragma once

#include "EngineTypes.h"

namespace A3
{
class VulkanRenderBackend;
class Scene;
class MeshObject;

class PathTracingRenderer
{
public:
	PathTracingRenderer( VulkanRenderBackend* inBackend );

	void beginFrame( int32 screenWidth, int32 screenHeight ) const;
	void render( const Scene& scene ) const;
	void endFrame() const;

private:
	void buildAccelerationStructure( const Scene& scene ) const;

private:
	VulkanRenderBackend* backend;
};
}
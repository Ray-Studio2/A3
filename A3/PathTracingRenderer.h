#pragma once

#include "EngineTypes.h"
#include "Shader.h"
#include "Material.h"
#include <memory>

namespace A3
{
class VulkanRenderBackend;
class Scene;
class MeshObject;
struct RaytracingPSO;

class PathTracingRenderer
{
public:
	PathTracingRenderer( VulkanRenderBackend* inBackend );
	~PathTracingRenderer();

	void beginFrame( int32 screenWidth, int32 screenHeight ) const;
	void render( const Scene& scene );
	void endFrame() const;

private:
	void buildSamplePSO( const Scene& scene );
	void buildAccelerationStructure( const Scene& scene ) const;

private:
	VulkanRenderBackend* backend;

	ShaderCache shaderCache;

	// @TODO: Move to global variable
	std::unique_ptr<RaytracingPSO> samplePSO;
};
}
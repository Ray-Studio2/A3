#pragma once

#include "EngineTypes.h"
#include "Shader.h"
#include "Vector.h"
#include "Matrix.h"
#include <memory>
#include <vector>

namespace A3
{
class VulkanRenderBackend;
class Scene;
class MeshObject;
struct RaytracingPSO;

struct LightData // TODO: scene or renderer?
{
	Vec3 emission = Vec3(5.0f);
	uint32 triangleCount = 0;
	Mat4x4 transform = Mat4x4::identity;
};

class PathTracingRenderer
{
public:
	PathTracingRenderer( VulkanRenderBackend* inBackend );
	~PathTracingRenderer();

	void beginFrame( int32 screenWidth, int32 screenHeight ) const;
	void render( Scene& scene );
	void endFrame() const;

private:
	void buildSamplePSO();
	void buildAccelerationStructure( const Scene& scene ) const;
	void updateLightBuffer( const Scene& scene );

private:
	VulkanRenderBackend* backend;

	ShaderCache shaderCache;

	// @TODO: Move to global variable
	std::unique_ptr<RaytracingPSO> samplePSO;
	
	// Variable for frame accumulation
	mutable uint32 frameCount = 0;
	
	// Light data
	std::vector<LightData> lights;
};
}
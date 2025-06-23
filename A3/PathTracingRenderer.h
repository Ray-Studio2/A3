#pragma once

#include "EngineTypes.h"
#include "Shader.h"
#include "Vector.h"
#include <memory>
#include <vector>

namespace A3
{
class VulkanRenderBackend;
class Scene;
class MeshObject;
struct RaytracingPSO;

struct LightData
{
	Vec3 position = Vec3(0, 0, 0);
	float radius = 1.0f;
	Vec3 emission = Vec3(0, 0, 0);
	float pad0 = 0.0f;
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
	
	// Auto-save settings
	//uint32 autoSaveFrameCount = 1000; // Save image at this frame count
	//bool autoSaveEnabled = true;
	
	// Light data
	std::vector<LightData> lights;
};
}
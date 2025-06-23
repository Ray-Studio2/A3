#include <unordered_map>
#include "PathTracingRenderer.h"
#include "RenderSettings.h"
#include "Vulkan.h"
#include "Scene.h"
#include "MeshObject.h"
#include "MeshResource.h"
#include "AccelerationStructure.h"
#include "PipelineStateObject.h"

using namespace A3;

PathTracingRenderer::PathTracingRenderer( VulkanRenderBackend* inBackend )
	: backend( inBackend )
    , samplePSO( new RaytracingPSO() )
{
    
}

PathTracingRenderer::~PathTracingRenderer() {}

void PathTracingRenderer::beginFrame( int32 screenWidth, int32 screenHeight ) const
{
    backend->beginFrame( screenWidth, screenHeight );
}

void PathTracingRenderer::render( Scene& scene )
{
    if( scene.isSceneDirty() )
    {
        // temp
        backend->tempScenePointer = &scene;
        buildAccelerationStructure( scene );
        updateLightBuffer( scene );

        buildSamplePSO();
        
        // Reset frame count when scene changes
        frameCount = 0;

        scene.cleanSceneDirty();
    }

    // Increment frame count
    frameCount++;
    
    // Pass frame count to backend
    backend->currentFrameCount = frameCount;
    
    backend->beginRaytracingPipeline( samplePSO->pipeline.get() );
}

void PathTracingRenderer::endFrame() const
{
    backend->endFrame();
}

// @NOTE: This is a function to create a sample PSO.
void PathTracingRenderer::buildSamplePSO()
{
    std::string shaderName = "shaders/SampleRaytracing.glsl";

    RaytracingPSODesc psoDesc;
    {
        std::string samplingMode;
        switch (backend->tempScenePointer->getImguiParam()->lightSamplingMode)
        {
        case imguiParam::BruteForce:
            samplingMode = "BRUTE_FORCE";
            break;
        case imguiParam::NEE:
            samplingMode = "NEE";
            break;
        }

        std::string lightSelection;
        switch (backend->tempScenePointer->getImguiParam()->lightSelection)
        {
        case imguiParam::LightOnly:
            lightSelection = "LIGHT_ONLY_";
            break;
        case imguiParam::EnvMap:
            lightSelection = "ENV_MAP_";
            break;
        case imguiParam::Both:
            lightSelection = "ENV_MAP_";
        }

        psoDesc.shaders.emplace_back( SS_RayGeneration, shaderName );
        psoDesc.shaders.emplace_back( SS_Miss, shaderName, lightSelection + "ENVIRONMENT" );
        psoDesc.shaders.emplace_back( SS_ClosestHit, shaderName, samplingMode);
        psoDesc.shaders.emplace_back( SS_Miss, shaderName, "SHADOW" );
        ShaderDesc& rayGeneration = psoDesc.shaders[ 0 ];
        rayGeneration.descriptors.emplace_back( SRD_AccelerationStructure, 0 );
        rayGeneration.descriptors.emplace_back( SRD_StorageImage, 1 );
        rayGeneration.descriptors.emplace_back( SRD_UniformBuffer, 2 );
        rayGeneration.descriptors.emplace_back( SRD_StorageBuffer, 4 ); // Light buffer
        rayGeneration.descriptors.emplace_back( SRD_StorageImage, 5 ); // Accumulation image
        rayGeneration.descriptors.emplace_back( SRD_UniformBuffer, 7 ); // Imgui parameters
        ShaderDesc& environmentMiss = psoDesc.shaders[1];
        environmentMiss.descriptors.emplace_back( SRD_ImageSampler, 6 );
        ShaderDesc& closestHit = psoDesc.shaders[ 2 ];
        closestHit.descriptors.emplace_back( SRD_AccelerationStructure, 0 );
        closestHit.descriptors.emplace_back( SRD_StorageBuffer, 3 );
        closestHit.descriptors.emplace_back( SRD_StorageBuffer, 4 ); // Light buffer
        closestHit.descriptors.emplace_back( SRD_UniformBuffer, 7 ); // Imgui parameters
    }

    samplePSO->shaders.resize( psoDesc.shaders.size() );
    for( int32 index = 0; index < psoDesc.shaders.size(); ++index )
    {
        const ShaderDesc& shaderDesc = psoDesc.shaders[ index ];
        samplePSO->shaders[ index ] = shaderCache.addShaderModule( shaderDesc, backend->createShaderModule( shaderDesc ) );
    }

    backend->createOutImage();
    backend->createAccumulationImage();
    backend->createUniformBuffer();

    samplePSO->pipeline = backend->createRayTracingPipeline( psoDesc, samplePSO.get() );
}

void PathTracingRenderer::buildAccelerationStructure( const Scene& scene ) const
{
    std::vector<MeshObject*> meshObjects = scene.collectMeshObjects();
    std::vector<BLASBatch*> batches;
    batches.resize( meshObjects.size() );

    for( int32 index = 0; index < meshObjects.size(); ++index )
    {
        meshObjects[ index ]->createRenderResources( backend );
        batches[ index ] = meshObjects[ index ]->getBLASBatch();
    }

    backend->createTLAS( batches );

    backend->rebuildAccelerationStructure();
}

void PathTracingRenderer::updateLightBuffer( const Scene& scene )
{
    lights.clear();
    
    // Collect all mesh objects that are lights
    std::vector<MeshObject*> meshObjects = scene.collectMeshObjects();
    
    for( size_t i = 0; i < meshObjects.size(); ++i )
    {
        MeshObject* meshObj = meshObjects[i];
        
        // Check if this is a light (hardcoded for now - later can check material)
        if( i == 6 ) // LIGHT_INSTANCE_INDEX
        {
            LightData light;
            light.emission = meshObj->getEmittance();
            light.triangleCount = meshObj->getResource()->triangleCount;
            light.transform = meshObj->getLocalToWorld();
            
            lights.push_back( light );
        }
    }
    
    // Update light buffer in backend
    backend->updateLightBuffer( lights );
}
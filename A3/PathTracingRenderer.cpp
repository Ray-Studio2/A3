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

void PathTracingRenderer::render( const Scene& scene )
{
    if( scene.isSceneDirty() )
    {
        // temp
        backend->tempScenePointer = &scene;
        buildAccelerationStructure( scene );

        buildSamplePSO();
    }

    backend->beginRaytracingPipeline( samplePSO->pipeline.get() );
}

void PathTracingRenderer::endFrame() const
{
    backend->endFrame();
}

// @NOTE: This is a function to create a sample PSO.
void PathTracingRenderer::buildSamplePSO()
{
    RaytracingPSODesc psoDesc;
    {
        psoDesc.shaders.emplace_back( SS_RayGeneration, "SampleRaytracing.glsl" );
        psoDesc.shaders.emplace_back( SS_Miss, "SampleRaytracing.glsl", "ENVIRONMENT" );
        psoDesc.shaders.emplace_back( SS_ClosestHit, "SampleRaytracing.glsl" );
        psoDesc.shaders.emplace_back( SS_Miss, "SampleRaytracing.glsl", "SHADOW" );
        ShaderDesc& rayGeneration = psoDesc.shaders[ 0 ];
        rayGeneration.descriptors.emplace_back( SRD_AccelerationStructure, 0 );
        rayGeneration.descriptors.emplace_back( SRD_StorageImage, 1 );
        rayGeneration.descriptors.emplace_back( SRD_UniformBuffer, 2 );
        ShaderDesc& closestHit = psoDesc.shaders[ 2 ];
        closestHit.descriptors.emplace_back( SRD_AccelerationStructure, 0 );
        closestHit.descriptors.emplace_back( SRD_StorageBuffer, 3 );
        closestHit.descriptors.emplace_back( SRD_StorageBuffer, 4 );
        closestHit.descriptors.emplace_back( SRD_StorageBuffer, 5 );
    }

    samplePSO->shaders.resize( psoDesc.shaders.size() );
    for( int32 index = 0; index < psoDesc.shaders.size(); ++index )
    {
        const ShaderDesc& shaderDesc = psoDesc.shaders[ index ];
        samplePSO->shaders[ index ] = shaderCache.addShaderModule( shaderDesc, backend->createShaderModule( shaderDesc ) );
    }

    backend->createOutImage();
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
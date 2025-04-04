#include "PathTracingRenderer.h"
#include "RenderSettings.h"
#include "Vulkan.h"
#include "Scene.h"
#include "MeshObject.h"
#include "MeshResource.h"
#include "AccelerationStructure.h"

using namespace A3;

PathTracingRenderer::PathTracingRenderer( VulkanRenderBackend* inBackend )
	: backend( inBackend )
{
}

void PathTracingRenderer::beginFrame( int32 screenWidth, int32 screenHeight ) const
{
    backend->beginFrame( screenWidth, screenHeight );
}

void PathTracingRenderer::render( const Scene& scene ) const
{
    if( scene.isSceneDirty() )
    {
        buildAccelerationStructure( scene );
    }

    backend->beginRaytracingPipeline();
}

void PathTracingRenderer::endFrame() const
{
    backend->endFrame();
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
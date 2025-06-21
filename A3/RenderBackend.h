#pragma once

#include "RenderResource.h"
#include "MeshResource.h"
#include "Shader.h"
#include "Matrix.h"
#include <vector>

namespace A3
{
struct BLASBatch;
struct RaytracingPSO;
struct RaytracingPSODesc;
struct LightData;

class IRenderBackend
{
public:
    virtual void beginFrame( int32 screenWidth, int32 screenHeight ) = 0;
    virtual void endFrame() = 0;

    virtual void beginRaytracingPipeline( IRenderPipeline* inPipeline ) = 0;

    virtual void rebuildAccelerationStructure() = 0;

    virtual IAccelerationStructureRef createBLAS(
        const std::vector<VertexPosition>& positionData,
        const std::vector<VertexAttributes>& attributeData,
        const std::vector<uint32>& indexData,
        const Mat3x4& transformData ) = 0;

    virtual void createTLAS( const std::vector<BLASBatch*>& batches ) = 0;

    virtual IShaderModuleRef createShaderModule( const ShaderDesc& desc ) = 0;

    virtual IRenderPipelineRef createRayTracingPipeline( const RaytracingPSODesc& psoDesc, RaytracingPSO* pso ) = 0;
    
    virtual void updateLightBuffer( const std::vector<LightData>& lights ) = 0;
};
}
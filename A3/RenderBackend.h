#pragma once

#include "RenderResource.h"
#include "MeshResource.h"
#include "Matrix.h"

namespace A3
{
struct BLASBatch;

class IRenderBackend
{
public:
    virtual void beginFrame( int32 screenWidth, int32 screenHeight ) = 0;
    virtual void endFrame() = 0;

    virtual void beginRaytracingPipeline() = 0;

    virtual void rebuildAccelerationStructure() = 0;

    virtual IAccelerationStructureRef createBLAS(
        const std::vector<VertexPosition>& positionData,
        const std::vector<VertexAttributes>& attributeData,
        const std::vector<uint32>& indexData,
        const Mat3x4& transformData ) = 0;

    virtual void createTLAS( const std::vector<BLASBatch*>& batches ) = 0;
};
}
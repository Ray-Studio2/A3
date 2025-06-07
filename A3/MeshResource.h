#pragma once

#include "SceneResource.h"
#include <vector>
#include "EngineTypes.h"

namespace A3
{
struct VertexPosition
{
    float x, y, z, w;
};

struct VertexAttributes
{
    float normals[ 4 ];
    float uvs[ 4 ];
};

struct MeshResource : public ISceneResource
{
    std::vector<VertexPosition> positions;
    std::vector<VertexAttributes> attributes;
    std::vector<uint32> indices;
};
}
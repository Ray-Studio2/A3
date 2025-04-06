#pragma once

#include "RenderResource.h"

namespace A3
{
enum ELogicalShaderType
{
    LST_Vertex          = 0,
    LST_Fragment        = 1,
    LST_Compute         = 2,
    LST_RayGeneration   = 3,
    LST_AnyHit          = 4,
    LST_ClosestHit      = 5,
    LST_Miss_Background = 6,
    LST_Miss_Shadow     = 7,
};

struct ShaderDesc
{
    ELogicalShaderType  type;
    std::string         fileName;
    
    // Does not support yet
    std::string entry;
};

class Shader
{
public:
    Shader() {}

private:
    IShaderModuleRef module;
};
}

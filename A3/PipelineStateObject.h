#pragma once

#include "Shader.h"
#include <vector>
#include <array>

namespace A3
{
struct GraphicsPSODesc
{

};

struct GraphicsPSO
{

};

struct ComputePSODesc
{

};

struct ComputePSO
{

};

// @TODO: Merge with PSO?
struct RaytracingPSODesc
{
	std::vector<ShaderDesc> shaders;
};

struct RaytracingPSO
{
	std::vector<IShaderModule*> shaders;

	IRenderPipelineRef pipeline;
};
}

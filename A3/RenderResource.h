#pragma once

#include <memory>

namespace A3
{
struct IAccelerationStructure
{
	virtual ~IAccelerationStructure() {}
};
using IAccelerationStructureRef = std::unique_ptr<IAccelerationStructure>;

struct IShader
{
	virtual ~IShader() {}
};
using IShaderRef = std::unique_ptr<IShader>;

struct IShaderInstance
{
	virtual ~IShaderInstance() {}
};
using IShaderInstanceRef = std::unique_ptr<IShaderInstance>;

struct IRenderPipeline
{
	virtual ~IRenderPipeline() {}
};
using IRenderPipelineRef = std::unique_ptr<IRenderPipeline>;
}
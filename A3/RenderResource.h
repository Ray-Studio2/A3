#pragma once

#include <memory>

namespace A3
{
struct IAccelerationStructure
{
	virtual ~IAccelerationStructure() {}

	virtual void destroy() = 0;
};
using IAccelerationStructureRef = std::unique_ptr<IAccelerationStructure>;

struct IShaderModule
{
	virtual ~IShaderModule() {}
};
using IShaderModuleRef = std::unique_ptr<IShaderModule>;

struct IRenderPipeline
{
	virtual ~IRenderPipeline() {}
};
using IRenderPipelineRef = std::unique_ptr<IRenderPipeline>;
}
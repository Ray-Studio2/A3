#pragma once

#include "RenderResource.h"
#include "Shader.h"
#include <vulkan/vulkan.h>

namespace A3
{
struct VulkanAccelerationStructure : public IAccelerationStructure
{
public:
	VulkanAccelerationStructure()
		: descriptor( nullptr )
		, memory( nullptr )
		, handle( nullptr )
	{}

	virtual ~VulkanAccelerationStructure()
	{}

public:
	VkBuffer					descriptor;
	VkDeviceMemory				memory;
	VkAccelerationStructureKHR	handle;
};

struct VulkanShaderModule : public IShaderModule
{
public:
	VulkanShaderModule()
		: module( nullptr )
	{}

public:
	VkShaderModule module;
};
}
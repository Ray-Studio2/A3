#pragma once

#include "RenderResource.h"
#include <vulkan/vulkan.h>

namespace A3
{
struct VulkanAccelerationStructure : public IAccelerationStructure
{
	VulkanAccelerationStructure()
		: descriptor( nullptr )
		, memory( nullptr )
		, handle( nullptr )
	{}

	virtual ~VulkanAccelerationStructure()
	{
	}

	VkBuffer					descriptor;
	VkDeviceMemory				memory;
	VkAccelerationStructureKHR	handle;
};
}
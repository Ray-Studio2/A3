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
    VkBuffer                    descriptor;
    VkDeviceMemory              memory;
    VkAccelerationStructureKHR  handle;

    VkBuffer vertexPositionBuffer;
    VkBuffer vertexAttributeBuffer;
    VkBuffer indexBuffer;
    VkBuffer cumulativeTriangleAreaBuffer;
    VkBuffer materialAddress;
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

struct VulkanPipeline : public IRenderPipeline
{
public:
    VulkanPipeline()
        : descriptorSetLayout( nullptr )
        , descriptorSet( nullptr )
        , pipelineLayout( nullptr )
        , pipeline( nullptr )
    {}

public:
    VkDescriptorSetLayout   descriptorSetLayout;
    VkDescriptorSet         descriptorSet;
    VkPipelineLayout        pipelineLayout;
    VkPipeline              pipeline;
};
}
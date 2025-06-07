#pragma once

#include "RenderResource.h"
#include "Shader.h"
#include <vulkan/vulkan.h>

namespace A3
{
enum EVulkanShaderBindingTableShaderGroup
{
    VSG_RayGeneration,
    VSG_Miss,
    VSG_Hit,
    VSG_Callable,
    VSG_COUNT
};

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
};

struct VulkanShader : public IShader
{
public:
    VulkanShader()
        : module( nullptr )
    {}

public:
    VkShaderModule module;
};

struct VulkanShaderInstance : public IShaderInstance
{
public:
    VulkanShaderInstance()
        : shader( nullptr )
    {}

public:
    VulkanShader* shader;

    std::vector<uint8> shaderRecordData;
};

struct VulkanPipeline : public IRenderPipeline
{
public:
    VulkanPipeline()
        : descriptorSetLayout( nullptr )
        , descriptorSet( nullptr )
        , pipelineLayout( nullptr )
        , pipeline( nullptr )
        , sbtDescriptor( nullptr )
        , sbtMemory( nullptr )
        , sbtAddresses()
    {}

public:
    VkDescriptorSetLayout   descriptorSetLayout;
    VkDescriptorSet         descriptorSet;
    VkPipelineLayout        pipelineLayout;
    VkPipeline              pipeline;

    VkBuffer        sbtDescriptor;
    VkDeviceMemory  sbtMemory;
    VkStridedDeviceAddressRegionKHR sbtAddresses[ VSG_COUNT ];
};
}
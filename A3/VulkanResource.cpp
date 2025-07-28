#include "VulkanResource.h"
#include "Vulkan.h"

extern VkDevice gDevice;
extern PFN_vkDestroyAccelerationStructureKHR gvkDestroyAccelerationStructureKHR;

namespace A3
{
    VulkanAccelerationStructure::~VulkanAccelerationStructure()
    {
    }
    void VulkanAccelerationStructure::destroy()
    {
        //VkBuffer                    descriptor;
        //VkDeviceMemory              memory;
        //VkAccelerationStructureKHR  handle;

        //VkBuffer vertexPositionBuffer;
        //VkBuffer vertexAttributeBuffer;
        //VkBuffer indexBuffer;
        //VkBuffer cumulativeTriangleAreaBuffer;
        //VkBuffer materialBuffer;

        vkDestroyBuffer(gDevice, descriptor, nullptr);
        vkFreeMemory(gDevice, memory, nullptr);
        gvkDestroyAccelerationStructureKHR(gDevice, handle, nullptr);

        vkDestroyBuffer(gDevice, vertexPositionBuffer, nullptr);
        vkDestroyBuffer(gDevice, vertexAttributeBuffer, nullptr);
        vkDestroyBuffer(gDevice, indexBuffer, nullptr);
        vkDestroyBuffer(gDevice, cumulativeTriangleAreaBuffer, nullptr);
        //vkDestroyBuffer(gDevice, materialBuffer, nullptr);
    }
}
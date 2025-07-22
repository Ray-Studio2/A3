#define _CRT_SECURE_NO_WARNINGS

#include "Vulkan.h"
//#include "shader_module.h"
#include "RenderSettings.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_glfw.h"
#include "ThirdParty/imgui/imgui_impl_vulkan.h"
#include "MeshResource.h"
#include "MeshObject.h"
#include "VulkanResource.h"
#include "AccelerationStructure.h"
#include "Shader.h"
#include "PipelineStateObject.h"
#include "PathTracingRenderer.h" // For LightData
#include <random>
#include <filesystem>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Vulkan error checking macro
#define VK_CHECK(x) \
    do { \
        VkResult err = x; \
        if (err != VK_SUCCESS) { \
            printf("Vulkan error in %s at line %d: %s (error code: %d)\n", \
                   __FILE__, __LINE__, #x, err); \
            assert(false); \
        } \
    } while (0)

const char* getVkResultString(VkResult result) {
    switch(result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default: return "Unknown VkResult";
    }
}

using namespace A3;

std::vector<TextureManager::TextureView> TextureManager::gTextureArray;
TextureParameter TextureManager::gWhiteParameter;
VkSampler TextureManager::gLinearSampler;

void TextureManager::initialize(VulkanRenderBackend& vulkanBackend)
{
    std::string whiteTextureName = "NullTexture_White";

    uint32 width = 2, height = 2;
    std::vector<float> pixels(width * height * 4, 0);
    for (uint32 i = 0; i < height; ++i)
    {
        for (uint32 j = 0; j < width; ++j)
        {
            pixels[i * (width * 4) + j * 4 + 0] = 1.0f;
            pixels[i * (width * 4) + j * 4 + 1] = 1.0f;
            pixels[i * (width * 4) + j * 4 + 2] = 1.0f;
            pixels[i * (width * 4) + j * 4 + 3] = 1.0f;
        }
    }

    gWhiteParameter = createTexture(vulkanBackend, whiteTextureName, static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), width, height, pixels.data());

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxLod = FLT_MAX,
    };
    vkCreateSampler(vulkanBackend.device, &samplerInfo, nullptr, &gLinearSampler);
}

const TextureParameter TextureManager::createTexture(VulkanRenderBackend& vulkanBackend, const std::string path, const uint32 imageFormat, const uint32 width, const uint32 height, const float* pixelData)
{
    uint32 index = 0;
    for (; index < gTextureArray.size(); ++index)
    {
        const TextureView& view = gTextureArray[index];
        if (view._path == path)
            return TextureParameter{ index };
    }

    TextureView view = vulkanBackend.createResourceImage(imageFormat, width, height, pixelData);
    view._path = path;
    gTextureArray.push_back(view);

    return TextureParameter{ index };
}

void A3::Material::uploadMaterialParameter(VulkanRenderBackend& vulkanBackend)
{
    void* dst;
    vkMapMemory(vulkanBackend.device, _buffer._memory, 0, sizeof(_parameter), 0, &dst);
    memcpy(dst, static_cast<void*>(&_parameter), sizeof(_parameter));
    vkUnmapMemory(vulkanBackend.device, _buffer._memory);
}

VulkanRenderBackend::VulkanRenderBackend( GLFWwindow* window, std::vector<const char*>& extensions, int32 screenWidth, int32 screenHeight )
    : lightBuffer(VK_NULL_HANDLE), lightBufferMem(VK_NULL_HANDLE)
{
    createVkInstance( extensions );
    createVkPhysicalDevice();
    createVkSurface( window );
    createVkQueueFamily();
    createVkDescriptorPools();
    createSwapChain();
    createImguiRenderPass( screenWidth, screenHeight );
    createCommandCenter();

    TextureManager::initialize(*this);

    //// 옮겨야함
    //createEnvironmentMap(RenderSettings::envMapPath);
}

VulkanRenderBackend::~VulkanRenderBackend()
{
    // @TODO: restore
    //vkDeviceWaitIdle( device );

    //vkDestroyBuffer( device, tlasBuffer, nullptr );
    //vkFreeMemory( device, tlasBufferMem, nullptr );
    //vkDestroyAccelerationStructureKHR( device, tlas, nullptr );

    //vkDestroyBuffer( device, blasBuffer, nullptr );
    //vkFreeMemory( device, blasBufferMem, nullptr );
    //vkDestroyAccelerationStructureKHR( device, blas, nullptr );

    //vkDestroyImageView( device, outImageView, nullptr );
    //vkDestroyImage( device, outImage, nullptr );
    //vkFreeMemory( device, outImageMem, nullptr );

    //vkDestroyBuffer( device, uniformBuffer, nullptr );
    //vkFreeMemory( device, uniformBufferMem, nullptr );

    //vkDestroyBuffer( device, sbtBuffer, nullptr );
    //vkFreeMemory( device, sbtBufferMem, nullptr );

    //vkDestroyPipeline( device, pipeline, nullptr );
    //vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
    //vkDestroyDescriptorSetLayout( device, descriptorSetLayout, nullptr );

    //vkDestroyDescriptorPool( device, descriptorPool, nullptr );


    //vkDestroySemaphore( device, renderFinishedSemaphore, nullptr );
    //vkDestroySemaphore( device, imageAvailableSemaphore, nullptr );
    //vkDestroyFence( device, fence0, nullptr );

    //vkDestroyCommandPool( device, commandPool, nullptr );

    //// for (auto imageView : swapChainImageViews) {
    ////     vkDestroyImageView(device, imageView, nullptr);
    //// }
    //vkDestroySwapchainKHR( device, swapChain, nullptr );
    //vkDestroyDevice( device, nullptr );
    //if( ON_DEBUG )
    //{
    //    ( ( PFN_vkDestroyDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" ) )
    //        ( instance, debugMessenger, nullptr );
    //}
    //vkDestroySurfaceKHR( instance, surface, nullptr );
    //vkDestroyInstance( instance, nullptr );
}

////////////////////////////////////////////////
// @FIXME: Remove duplicate
////////////////////////////////////////////////
static void check_vk_result( VkResult err )
{
    if( err == VK_SUCCESS )
        return;
    fprintf( stderr, "[vulkan] Error: VkResult = %d\n", err );
    if( err < 0 )
        abort();
}

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData )
{
    ( void )flags; ( void )object; ( void )location; ( void )messageCode; ( void )pUserData; ( void )pLayerPrefix; // Unused arguments
    fprintf( stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
    return VK_FALSE;
}
#endif // APP_USE_VULKAN_DEBUG_REPORT

static bool IsExtensionAvailable( const std::vector<VkExtensionProperties>& properties, const char* extension )
{
    for( const VkExtensionProperties& p : properties )
        if( strcmp( p.extensionName, extension ) == 0 )
            return true;
    return false;
}
////////////////////////////////////////////////

#define TEXTUREBINDLESS_BINDING_LOCATION 10
#define TEXTUREBINDLESS_BINDING_MAX_COUNT 1024

void VulkanRenderBackend::beginFrame( int32 screenWidth, int32 screenHeight )
{
    VkSemaphore image_acquired_semaphore = imageAvailableSemaphores[ semaphoreIndex ];
    VkSemaphore render_complete_semaphore = renderFinishedSemaphores[ semaphoreIndex ];
    VkResult err = vkAcquireNextImageKHR( device, swapChain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &imageIndex );

    {
        VkResult err;
        err = vkWaitForFences(device, 1, &fences[imageIndex], VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(device, 1, &fences[imageIndex]);
        check_vk_result(err);
    }
}

void VulkanRenderBackend::endFrame()
{
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores[ semaphoreIndex ],
        .swapchainCount = 1,
        .pSwapchains = &swapChain,
        .pImageIndices = &imageIndex,
    };

    vkQueuePresentKHR(graphicsQueue, &presentInfo);

    semaphoreIndex = (semaphoreIndex + 1) % 3;
}

void VulkanRenderBackend::beginRaytracingPipeline( IRenderPipeline* inPipeline )
{
    VulkanPipeline* pipeline = static_cast< VulkanPipeline* >( inPipeline );

    // Update uniform buffer (including frame count)
    updateCameraBuffer();

    VkCommandBufferBeginInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer( commandBuffers[ imageIndex ], &info );

    vkCmdBindPipeline( commandBuffers[ imageIndex ], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline->pipeline );
    vkCmdBindDescriptorSets(
        commandBuffers[ imageIndex ], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->pipelineLayout, 0, 1, &pipeline->descriptorSet, 0, 0 );

    vkCmdTraceRaysKHR(
        commandBuffers[ imageIndex ],
        &rgenSbt,
        &missSbt,
        &hitgSbt,
        &callSbt,
        RenderSettings::screenWidth, RenderSettings::screenHeight, 1 );

    setImageLayout(
        commandBuffers[ imageIndex ],
        outImage,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        subresourceRange );

    setImageLayout(
        commandBuffers[ imageIndex ],
        swapChainImages[ imageIndex ],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange );

    vkCmdCopyImage(
        commandBuffers[ imageIndex ],
        outImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapChainImages[ imageIndex ], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion );

    setImageLayout(
        commandBuffers[ imageIndex ],
        outImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        subresourceRange );

    setImageLayout(
        commandBuffers[ imageIndex ],
        swapChainImages[ imageIndex ],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        subresourceRange );

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores[ semaphoreIndex ],
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[ imageIndex ],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &rtFinishedSemaphores[ semaphoreIndex ],
    };
    //VkSubmitInfo submitInfo{
    //    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    //    .commandBufferCount = 1,
    //    .pCommandBuffers = &commandBuffers[imageIndex],
    //};

    VkResult endCmdResult = vkEndCommandBuffer( commandBuffers[ imageIndex ] );
    if (endCmdResult != VK_SUCCESS) {
        printf("ERROR: vkEndCommandBuffer failed with error: %s\n", getVkResultString(endCmdResult));
    }

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, fences[imageIndex]);

    //VkResult submitResult = vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    //if (submitResult != VK_SUCCESS) {
    //    printf("ERROR: vkQueueSubmit failed with error: %s (frame: %d)\n", getVkResultString(submitResult), currentFrameCount);
    //    if (submitResult == VK_ERROR_DEVICE_LOST) {
    //        printf("Device lost during ray tracing submit!\n");
    //    }
    //}
    //
    //VkResult waitResult = vkQueueWaitIdle(graphicsQueue);
    //if (waitResult != VK_SUCCESS) {
    //    printf("ERROR: vkQueueWaitIdle failed with error: %s\n", getVkResultString(waitResult));
    //}
}

void VulkanRenderBackend::rebuildAccelerationStructure()
{
    createOutImage();
    createAccumulationImage();
    createUniformBuffer();
    createLightBuffer();
    createEnvironmentMap(RenderSettings::envMapPath);
}

void VulkanRenderBackend::loadDeviceExtensionFunctions( VkDevice device )
{
    vkGetBufferDeviceAddressKHR = ( PFN_vkGetBufferDeviceAddressKHR )( vkGetDeviceProcAddr( device, "vkGetBufferDeviceAddressKHR" ) );
    vkGetAccelerationStructureDeviceAddressKHR = ( PFN_vkGetAccelerationStructureDeviceAddressKHR )( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureDeviceAddressKHR" ) );
    vkCreateAccelerationStructureKHR = ( PFN_vkCreateAccelerationStructureKHR )( vkGetDeviceProcAddr( device, "vkCreateAccelerationStructureKHR" ) );
    vkDestroyAccelerationStructureKHR = ( PFN_vkDestroyAccelerationStructureKHR )( vkGetDeviceProcAddr( device, "vkDestroyAccelerationStructureKHR" ) );
    vkGetAccelerationStructureBuildSizesKHR = ( PFN_vkGetAccelerationStructureBuildSizesKHR )( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureBuildSizesKHR" ) );
    vkCmdBuildAccelerationStructuresKHR = ( PFN_vkCmdBuildAccelerationStructuresKHR )( vkGetDeviceProcAddr( device, "vkCmdBuildAccelerationStructuresKHR" ) );
    vkCreateRayTracingPipelinesKHR = ( PFN_vkCreateRayTracingPipelinesKHR )( vkGetDeviceProcAddr( device, "vkCreateRayTracingPipelinesKHR" ) );
    vkGetRayTracingShaderGroupHandlesKHR = ( PFN_vkGetRayTracingShaderGroupHandlesKHR )( vkGetDeviceProcAddr( device, "vkGetRayTracingShaderGroupHandlesKHR" ) );
    vkCmdTraceRaysKHR = ( PFN_vkCmdTraceRaysKHR )( vkGetDeviceProcAddr( device, "vkCmdTraceRaysKHR" ) );

    VkPhysicalDeviceProperties2 deviceProperties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties,
    };
    vkGetPhysicalDeviceProperties2( physicalDevice, &deviceProperties2 );

    if( rtProperties.shaderGroupHandleSize != RenderSettings::shaderGroupHandleSize )
    {
        throw std::runtime_error( "shaderGroupHandleSize must be 32 mentioned in the vulakn spec (Table 69. Required Limits)!" );
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderBackend::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData )
{
    const char* severity;
    switch( messageSeverity )
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[Verbose]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[Warning]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[Error]"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "[Info]"; break;
        default: severity = "[Unknown]";
    }

    const char* types;
    switch( messageType )
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: types = "[General]"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: types = "[Performance]"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: types = "[Validation]"; break;
        default: types = "[Unknown]";
    }

    std::cout << "[Debug]" << severity << types << pCallbackData->pMessage << std::endl;

    // Print additional information for errors
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cout << "  Message ID: " << pCallbackData->pMessageIdName << std::endl;
        std::cout << "  Message ID Number: " << pCallbackData->messageIdNumber << std::endl;

        if (pCallbackData->queueLabelCount > 0) {
            std::cout << "  Queue Labels:" << std::endl;
            for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++) {
                std::cout << "    " << pCallbackData->pQueueLabels[i].pLabelName << std::endl;
            }
        }

        if (pCallbackData->cmdBufLabelCount > 0) {
            std::cout << "  Command Buffer Labels:" << std::endl;
            for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
                std::cout << "    " << pCallbackData->pCmdBufLabels[i].pLabelName << std::endl;
            }
        }

        if (pCallbackData->objectCount > 0) {
            std::cout << "  Objects:" << std::endl;
            for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
                std::cout << "    Type: " << pCallbackData->pObjects[i].objectType
                          << ", Handle: " << pCallbackData->pObjects[i].objectHandle;
                if (pCallbackData->pObjects[i].pObjectName) {
                    std::cout << ", Name: " << pCallbackData->pObjects[i].pObjectName;
                }
                std::cout << std::endl;
            }
        }

        // Break on error for debugging
        #ifdef _DEBUG
        // __debugbreak();
        #endif
    }

    return VK_FALSE;
}

bool VulkanRenderBackend::checkValidationLayerSupport( std::vector<const char*>& reqestNames )
{
    uint32_t count;
    vkEnumerateInstanceLayerProperties( &count, nullptr );
    std::vector<VkLayerProperties> availables( count );
    vkEnumerateInstanceLayerProperties( &count, availables.data() );

    for( const char* reqestName : reqestNames )
    {
        bool found = false;

        for( const auto& available : availables )
        {
            if( strcmp( reqestName, available.layerName ) == 0 )
            {
                found = true;
                break;
            }
        }

        if( !found )
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderBackend::checkDeviceExtensionSupport( VkPhysicalDevice device, std::vector<const char*>& reqestNames )
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties( device, nullptr, &count, nullptr );
    std::vector<VkExtensionProperties> availables( count );
    vkEnumerateDeviceExtensionProperties( device, nullptr, &count, availables.data() );

    for( const char* reqestName : reqestNames )
    {
        bool found = false;

        for( const auto& available : availables )
        {
            if( strcmp( reqestName, available.extensionName ) == 0 )
            {
                found = true;
                break;
            }
        }

        if( !found )
        {
            return false;
        }
    }

    return true;
}

void VulkanRenderBackend::createVkInstance( std::vector<const char*>& extensions )
{
    VkResult err;

    VkApplicationInfo appInfo
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> validationLayers;
    if( ON_DEBUG ) validationLayers.push_back( "VK_LAYER_KHRONOS_validation" );
    if( !checkValidationLayerSupport( validationLayers ) )
    {
        throw std::runtime_error( "validation layers requested, but not available!" );
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |*/
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };

    VkInstanceCreateFlags flags = {};
    {
        // Enumerate available extensions
        uint32_t properties_count;
        std::vector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, nullptr );
        properties.resize( properties_count );
        err = vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, properties.data() );
        check_vk_result( err );

        // Enable required extensions
        if( IsExtensionAvailable( properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) )
            extensions.push_back( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME );
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if( IsExtensionAvailable( properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) )
        {
            extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif
    }

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = ON_DEBUG ? &debugCreateInfo : nullptr,
        .pApplicationInfo = &appInfo,
        // @NOTE: Disable these lines to attach Nvidia Nsight
        .enabledLayerCount = ( uint32 )validationLayers.size(),
        .ppEnabledLayerNames = validationLayers.data(),
        //
        .enabledExtensionCount = ( uint32 )extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    if( vkCreateInstance( &createInfo, nullptr, &instance ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create instance!" );
    }

    if( ON_DEBUG )
    {
        auto func = ( PFN_vkCreateDebugUtilsMessengerEXT )vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
        if( !func || func( instance, &debugCreateInfo, nullptr, &debugMessenger ) != VK_SUCCESS )
        {
            throw std::runtime_error( "failed to set up debug messenger!" );
        }
    }
}

void VulkanRenderBackend::createVkSurface( GLFWwindow* window )
{
    if( glfwCreateWindowSurface( instance, window, allocator, &surface ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create window surface!" );
    }

    // Check for WSI support
    VkBool32 bSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, queueFamilyIndex, surface, &bSupported );
    if( !bSupported )
    {
        throw std::runtime_error( "failed to create window surface!" );
    }
}

std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,

    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // not used
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    //VK_KHR_RAY_QUERY_EXTENSION_NAME,
    //"VK_EXT_samplerless_texture_functions",

    VK_KHR_SPIRV_1_4_EXTENSION_NAME, // not used
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
};

void VulkanRenderBackend::createVkPhysicalDevice()
{
    physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );
    std::vector<VkPhysicalDevice> devices( deviceCount );
    vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() );

    for( const auto& device : devices )
    {
        if( checkDeviceExtensionSupport( device, deviceExtensions ) )
        {
            physicalDevice = device;
            break;
        }
    }

    if( physicalDevice == VK_NULL_HANDLE )
    {
        throw std::runtime_error( "failed to find a suitable GPU!" );
    }
}

void VulkanRenderBackend::createVkQueueFamily()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamilyCount, nullptr );
    std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamilyCount, queueFamilies.data() );

    queueFamilyIndex = 0;
    {
        for( ; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex )
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, queueFamilyIndex, surface, &presentSupport );

            if( queueFamilies[ queueFamilyIndex ].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport )
                break;
        }

        if( queueFamilyIndex >= queueFamilyCount )
            throw std::runtime_error( "failed to find a graphics & present queue!" );
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeat{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceRayQueryFeaturesKHR rqFeat{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .rayQuery = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE,
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpFeat{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE,
    };

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 feats2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
    };

    feats2.pNext = &bdaFeat;
    bdaFeat.pNext = &rqFeat;
    rqFeat.pNext = &asFeat;
    asFeat.pNext = &rtpFeat;
    rtpFeat.pNext = &descriptorIndexingFeatures;
    descriptorIndexingFeatures.pNext = nullptr;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &feats2);

    // Non-uniform indexing and update after bind
    // binding flags for textures, uniforms, and buffers
    // are required for our extension
    assert(descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
    assert(descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind);
    assert(descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing);
    assert(descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind);
    assert(descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing);
    assert(descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind);

    //if (!rqFeat.rayQuery)
    //    throw std::runtime_error("Device doesn't support VK_KHR_ray_query");

    feats2.features.shaderInt64 = VK_TRUE;
    bdaFeat.bufferDeviceAddress = VK_TRUE;
    rqFeat.rayQuery = VK_TRUE;
    asFeat.accelerationStructure = VK_TRUE;
    rtpFeat.rayTracingPipeline = VK_TRUE;    

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &feats2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = (uint32)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);

    // @TODO: Isolate function
    loadDeviceExtensionFunctions(device);
}

void VulkanRenderBackend::createVkDescriptorPools()
{
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, TEXTUREBINDLESS_BINDING_MAX_COUNT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    // Create Descriptor Pool
    // If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
    {
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;// VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for( VkDescriptorPoolSize& pool_size : poolSizes )
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = ( uint32_t )IM_ARRAYSIZE( poolSizes );
        pool_info.pPoolSizes = poolSizes;
        vkCreateDescriptorPool( device, &pool_info, allocator, &descriptorPool );
    }
}

void VulkanRenderBackend::createSwapChain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &capabilities );

    const VkColorSpaceKHR defaultSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;// VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    {
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, nullptr );
        std::vector<VkSurfaceFormatKHR> formats( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, formats.data() );

        bool found = false;
        for( auto format : formats )
        {
            if( format.format == swapChainImageFormat && format.colorSpace == defaultSpace )
            {
                found = true;
                break;
            }
        }

        if( !found )
        {
            throw std::runtime_error( "" );
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, nullptr );
        std::vector<VkPresentModeKHR> presentModes( presentModeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, presentModes.data() );

        for( auto mode : presentModes )
        {
            if( mode == VK_PRESENT_MODE_MAILBOX_KHR )
            {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    uint32 imageCount = 3;// capabilities.minImageCount + 1;
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = swapChainImageFormat,
        .imageColorSpace = defaultSpace,
        .imageExtent = {.width = RenderSettings::screenWidth , .height = RenderSettings::screenHeight },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };

    if( vkCreateSwapchainKHR( device, &createInfo, nullptr, &swapChain ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create swap chain!" );
    }

    vkGetSwapchainImagesKHR( device, swapChain, &imageCount, nullptr );
    swapChainImages.resize( imageCount );
    vkGetSwapchainImagesKHR( device, swapChain, &imageCount, swapChainImages.data() );

    for( const auto& image : swapChainImages )
    {
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapChainImageFormat,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        // VkImageView imageView;
        // if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to create image views!");
        // }
        // swapChainImageViews.push_back(imageView);
    }
}

void VulkanRenderBackend::createImguiRenderPass( int32 screenWidth, int32 screenHeight )
{
    VkResult err;

    // Create the Render Pass
    {
        VkAttachmentDescription attachment
        {
            .format = swapChainImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkAttachmentReference color_attachment
        {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment
        };

        VkSubpassDependency dependency
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        };

        VkRenderPassCreateInfo info
        {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency
        };

        err = vkCreateRenderPass( device, &info, allocator, &imguiRenderPass );
        check_vk_result( err );
    }

    // Create The Image Views
    {
        VkImageViewCreateInfo info
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapChainImageFormat,
            .components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
            .subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };

        swapChainImageViews.resize( swapChainImages.size() );
        for( uint32_t i = 0; i < swapChainImages.size(); i++ )
        {
            info.image = swapChainImages[ i ];
            err = vkCreateImageView( device, &info, allocator, &swapChainImageViews[ i ] );
            check_vk_result( err );
        }
    }

    // Create Framebuffer
    {
        VkImageView attachment[ 1 ];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = imguiRenderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = screenWidth;
        info.height = screenHeight;
        info.layers = 1;

        framebuffers.resize( swapChainImages.size() );
        for( uint32_t i = 0; i < swapChainImages.size(); i++ )
        {
            attachment[ 0 ] = swapChainImageViews[ i ];
            err = vkCreateFramebuffer( device, &info, allocator, &framebuffers[ i ] );
            check_vk_result( err );
        }
    }
}

void VulkanRenderBackend::createCommandCenter()
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    commandPools.resize( swapChainImages.size() );
    commandBuffers.resize( swapChainImages.size() );
    imageAvailableSemaphores.resize( swapChainImages.size() );
    rtFinishedSemaphores.resize( swapChainImages.size() );
    renderFinishedSemaphores.resize( swapChainImages.size() );
    fences.resize( swapChainImages.size() );

    for( uint32 i = 0; i < swapChainImages.size(); ++i )
    {
        if( vkCreateCommandPool( device, &poolInfo, nullptr, &commandPools[ i ] ) != VK_SUCCESS )
        {
            throw std::runtime_error( "failed to create command pool!" );
        }

        allocInfo.commandPool = commandPools[ i ];
        if( vkAllocateCommandBuffers( device, &allocInfo, &commandBuffers[ i ] ) != VK_SUCCESS )
        {
            throw std::runtime_error( "failed to allocate command buffers!" );
        }

        if( vkCreateSemaphore( device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[ i ]) != VK_SUCCESS ||
            vkCreateSemaphore( device, &semaphoreInfo, nullptr, &rtFinishedSemaphores[ i ]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence( device, &fenceInfo, nullptr, &fences[ i ] ) != VK_SUCCESS )
        {
            throw std::runtime_error( "failed to create synchronization objects for a frame!" );
        }
    }
}

void A3::VulkanRenderBackend::createEnvironmentMap(std::string_view hdrTexturePath)
{
    int width, height, channels;
    if (hdrTexturePath.empty())
        hdrTexturePath = RenderSettings::envMapDefault;
    float* pixels = stbi_loadf(hdrTexturePath.data(), &width, &height, &channels, 0);
    assert(pixels && channels == 3);

    VkDeviceSize imageSize = width * height * 4 * sizeof(float);
    std::vector<float> rgbaPixels(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        rgbaPixels[i * 4 + 0] = pixels[i * 3 + 0];
        rgbaPixels[i * 4 + 1] = pixels[i * 3 + 1];
        rgbaPixels[i * 4 + 2] = pixels[i * 3 + 2];
        rgbaPixels[i * 4 + 3] = 1.0f;
    }

    TextureManager::TextureView envTexture = createResourceImage(static_cast<uint32>(VK_FORMAT_R32G32B32A32_SFLOAT), width, height, rgbaPixels.data());
    envImage = envTexture._image;
    envImageMem = envTexture._memory;
    envImageView = envTexture._view;

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxLod = FLT_MAX,
    };
    vkCreateSampler( device, &samplerInfo, nullptr, &envSampler );

    createEnvironmentMapImportanceSampling(pixels, width, height);
    stbi_image_free(pixels);
}

TextureManager::TextureView VulkanRenderBackend::createResourceImage(const uint32 imageFormat, const uint32 width, const uint32 height, const float* pixelData)
{
    TextureManager::TextureView textureView;

    VkFormat format = static_cast<VkFormat>(imageFormat);

    vkQueueWaitIdle(graphicsQueue);

    std::tie(textureView._image, textureView._memory) = createImage(
        { (uint32_t)width, (uint32_t)height },
        format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceSize imageSize = width * height * sizeof(float);
    if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
    {
        imageSize *= 4;
    }
    else if (format == VK_FORMAT_R32G32B32_SFLOAT)
    {
        imageSize *= 3;
    }
    else
    {
        // not support image type
        return textureView;
    }

    auto [stagingBuffer, stagingMem] = createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
    memcpy(data, pixelData, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMem);

    VkCommandBuffer& cmd = commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    setImageLayout(cmd, textureView._image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyBufferToImage(cmd, stagingBuffer, textureView._image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    setImageLayout(cmd, textureView._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = textureView._image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = subresourceRange,
    };
    vkCreateImageView(device, &viewInfo, nullptr, &textureView._view);

    return textureView;
}

void VulkanRenderBackend::createEnvironmentMapImportanceSampling(float* pixels, int width, int height)
{
    if (pixels == nullptr) return;

    struct EnvImportanceSampleData
    {
        Vec3 dir;
        float pdf;
    };

    const uint32 texelCount = width * height;
    std::vector<EnvImportanceSampleData> EnvData(texelCount);

    std::vector<float> luminances(texelCount);
    std::vector<float> sinTheta(height);
    std::vector<float> marginalPdf(height);
    std::vector<float> marginalCdf(height);
    std::vector<float> conditionalCdf( texelCount );
    std::vector<float> totalPdf( texelCount );

    float luminanceSum = 0.0f;

    // 1. Luminance × sin(theta) 계산 (soften: gamma 적용)
    const float gamma = 0.9f;
    for (int y = 0; y < height; ++y) {
        float theta = 3.14159265f * (y + 0.5f) / float(height);
        sinTheta[y] = std::sin(theta);
        for (int x = 0; x < width; ++x) {
            int i = y * width + x;
            float r = pixels[i * 3 + 0];
            float g = pixels[i * 3 + 1];
            float b = pixels[i * 3 + 2];
            float lum = std::pow(0.2126f * r + 0.7152f * g + 0.0722f * b, gamma);
            luminances[i] = lum * sinTheta[y];
            luminanceSum += luminances[i];
        }
    }

    if (luminanceSum <= 0.0f) luminanceSum = 1e-6f; // TODO: throw an exception instead

    // 2. Marginal PDF & CDF (y 방향)
    float marginalAccum = 0.0f;
    for (int y = 0; y < height; ++y) {
        float rowSum = 0.0f;
        for (int x = 0; x < width; ++x) {
            rowSum += luminances[y * width + x];
        }
        float pdf = rowSum / luminanceSum;
        //pdf = std::max(pdf, 1e-6f); // clamp 최소값
        marginalPdf[y] = pdf;
        marginalAccum += pdf;
        marginalCdf[y] = marginalAccum;
    }

    // normalize CDF
    for (int y = 0; y < height; ++y) {
        marginalCdf[y] /= marginalAccum;
    }
    marginalCdf[height - 1] = 1.0f; // 강제 클램프

    // 3. Conditional PDF & CDF (x 방향 per row)
    for (int y = 0; y < height; ++y) {
        float rowSum = 0.0f;
        for (int x = 0; x < width; ++x) {
            rowSum += luminances[y * width + x];
        }

        rowSum = std::max(rowSum, 1e-6f);
        float accum = 0.0f;

        // CDF 작성
        for (int x = 0; x < width; ++x) {
            int i = y * width + x;
            float conditionalPdf = luminances[i] / rowSum;
            //conditionalPdf = std::max( conditionalPdf, 1e-6f); // clamp
            accum += conditionalPdf;

            conditionalCdf[ i ] = accum;
            totalPdf[ i ] = conditionalPdf * marginalPdf[ y ];
        }

        // normalize conditional CDF
        for (int x = 0; x < width; ++x) {
            int i = y * width + x;
            conditionalCdf[ i ] /= accum;
        }

        // 마지막 CDF 클램프
        int last = y * width + (width - 1);
        conditionalCdf[ last ] = 1.0f;
    }

    constexpr float pi = 3.1415926535897932384626433832795;

    // ---- Marginal CDF 이진 탐색 (Y 방향) ----
    for( uint32 indexY = 0; indexY < height; ++indexY )
    {
        const float indexYNormalized = (float(indexY) + 0.5) / height;
        uint32 yLow = 0;
        uint32 yHigh = height - 1;
        while( yLow < yHigh )
        {
            const uint32 yMid = ( yLow + yHigh ) / 2;
            const float cdf = marginalCdf[ yMid ];
            if( cdf < indexYNormalized )
                yLow = yMid + 1;
            else
                yHigh = yMid;
        }
        const uint32 y = yLow;

        // ---- Conditional CDF 이진 탐색 (X 방향) ----
        for( uint32 indexX = 0; indexX < width; ++indexX )
        {
            const uint32 indexXY = indexX + indexY * width;
            const float indexXNormalized = (float(indexX) + 0.5) / width;
            uint32 xLow = 0;
            uint32 xHigh = width - 1;
            while( xLow < xHigh )
            {
                const uint32 xMid = ( xLow + xHigh ) / 2;
                const float cdf = conditionalCdf[ y * width + xMid ];
                if( cdf < indexXNormalized )
                    xLow = xMid + 1;
                else
                    xHigh = xMid;
            }
            const uint32 x = xLow;

            // ---- UV to Direction ----
            const float u = (x + 0.5) / float(width);
            const float v = (y + 0.5) / float(height);
            const float phi = 2.0 * pi * u;
            const float theta = pi * v;
            const float sinTheta = sin( theta );

            // ---- PDF with solid angle correction ----
            const float texelPdf = totalPdf[ y * width + x ];
            EnvData[ indexXY ].pdf = width * height * texelPdf / ( 2.0 * pi * pi * sinTheta );

            EnvData[ indexXY ].dir = Vec3( sinTheta * cos( phi ), cos( theta ), sinTheta * sin( phi ) );
        }
    }

    // 4. Vulkan Buffer 업로드
    // Envmap Sampling Image
    VkDeviceSize imageSize = sizeof( EnvImportanceSampleData ) * EnvData.size();

    std::tie( envImportanceImage, envImportanceMem ) = createImage(
        { ( uint32 )width, ( uint32 )height },
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    auto [stagingBuffer, stagingMem] = createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    void* data;
    vkMapMemory( device, stagingMem, 0, imageSize, 0, &data );
    memcpy( data, EnvData.data(), imageSize );
    vkUnmapMemory( device, stagingMem );

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = envImportanceImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = subresourceRange,
    };
    vkCreateImageView( device, &viewInfo, nullptr, &envImportanceView );

    // Envmap Hit Image
    VkDeviceSize imageSizeHit = sizeof(totalPdf[0]) * totalPdf.size();

    std::tie(envHitImage, envHitMem) = createImage(
        { (uint32)width, (uint32)height },
        VK_FORMAT_R32_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto [hitStagingBuffer, hitStagingMem] = createBuffer(
        imageSizeHit,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory(device, hitStagingMem, 0, imageSizeHit, 0, &data);
    memcpy(data, totalPdf.data(), imageSizeHit);
    vkUnmapMemory(device, hitStagingMem);

    VkImageViewCreateInfo hitViewInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = envHitImage,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = VK_FORMAT_R32_SFLOAT,
    .subresourceRange = subresourceRange,
    };
    vkCreateImageView(device, &hitViewInfo, nullptr, &envHitView);

    VkCommandBuffer& cmd = commandBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Importance(RGBA32F) 업로드 -------------------------------------------------
    setImageLayout(cmd, envImportanceImage, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyBufferToImage(cmd, stagingBuffer, envImportanceImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // HitPdf(R32F) 업로드 --------------------------------------------------------
    setImageLayout(cmd, envHitImage, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy hitRegion{};
    hitRegion.bufferOffset = 0;
    hitRegion.imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    hitRegion.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyBufferToImage(cmd, hitStagingBuffer, envHitImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &hitRegion);

    // Shader-read 전환 ----------------------------------------------------------
    setImageLayout(cmd, envImportanceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    setImageLayout(cmd, envHitImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    // 커맨드버퍼 종료 & Submit ----------------------------------------------------
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
    vkDestroyBuffer(device, hitStagingBuffer, nullptr);
    vkFreeMemory(device, hitStagingMem, nullptr);
}

uint32 VulkanRenderBackend::findMemoryType( uint32_t memoryTypeBits, VkMemoryPropertyFlags reqMemProps )
{
    uint32 memTypeIndex = 0;
    std::bitset<32> isSuppoted( memoryTypeBits );

    VkPhysicalDeviceMemoryProperties spec;
    vkGetPhysicalDeviceMemoryProperties( physicalDevice, &spec );

    for( auto& [props, _] : std::span<VkMemoryType>( spec.memoryTypes, spec.memoryTypeCount ) )
    {
        if( isSuppoted[ memTypeIndex ] && ( props & reqMemProps ) == reqMemProps )
        {
            break;
        }
        ++memTypeIndex;
    }
    return memTypeIndex;
}

std::tuple<VkBuffer, VkDeviceMemory> VulkanRenderBackend::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags reqMemProps )
{
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };
    if( vkCreateBuffer( device, &bufferInfo, nullptr, &buffer ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create vertex buffer!" );
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements( device, buffer, &memRequirements );

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, reqMemProps ),
    };

    if( usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT )
    {
        static VkMemoryAllocateFlagsInfo flagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
        };
        allocInfo.pNext = &flagsInfo;
    }

    if( vkAllocateMemory( device, &allocInfo, nullptr, &bufferMemory ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to allocate vertex buffer memory!" );
    }

    vkBindBufferMemory( device, buffer, bufferMemory, 0 );

    return { buffer, bufferMemory };
}

std::tuple<VkImage, VkDeviceMemory> VulkanRenderBackend::createImage(
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags reqMemProps )
{
    VkImage image;
    VkDeviceMemory imageMemory;

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if( vkCreateImage( device, &imageInfo, nullptr, &image ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create image!" );
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements( device, image, &memRequirements );

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType( memRequirements.memoryTypeBits, reqMemProps ),
    };

    if( vkAllocateMemory( device, &allocInfo, nullptr, &imageMemory ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to allocate image memory!" );
    }

    vkBindImageMemory( device, image, imageMemory, 0 );

    return { image, imageMemory };
}

void VulkanRenderBackend::setImageLayout(
    VkCommandBuffer cmdbuffer,
    VkImage image,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask )
{
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldImageLayout,
        .newLayout = newImageLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresourceRange,
    };

    if( oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL )
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    else if( oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if( newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL )
    {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    else if( newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL )
    {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    vkCmdPipelineBarrier(
        cmdbuffer, srcStageMask, dstStageMask, 0,
        0, nullptr, 0, nullptr, 1, &barrier );
}

inline VkDeviceAddress VulkanRenderBackend::getDeviceAddressOf( VkBuffer buffer )
{
    VkBufferDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return vkGetBufferDeviceAddressKHR( device, &info );
}

inline VkDeviceAddress VulkanRenderBackend::getDeviceAddressOf( VkAccelerationStructureKHR as )
{
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as,
    };
    return vkGetAccelerationStructureDeviceAddressKHR( device, &info );
}

// @TODO: Support more than 1 geometry
IAccelerationStructureRef VulkanRenderBackend::createBLAS( const BLASBuildParams params )
{
    VulkanAccelerationStructure* outBlas = new VulkanAccelerationStructure();
    VkDeviceMemory vertexPositionBufferMem;
    VkDeviceMemory vertexAttributeBufferMem;
    VkDeviceMemory indexBufferMem;
    VkDeviceMemory cumulativeTriangleAreaMem;

    auto& positionData = params.positionData;
    auto& attributeData = params.attributeData;
    auto& indexData = params.indexData;
    auto& cumulativeTriangleAreaData = params.cumulativeTriangleAreaData;
    auto& transformData = params.transformData;

    std::tie(outBlas->vertexPositionBuffer, vertexPositionBufferMem ) = createBuffer(
        positionData.size() * sizeof( VertexPosition ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    std::tie(outBlas->vertexAttributeBuffer, vertexAttributeBufferMem ) = createBuffer(
        attributeData.size() * sizeof( VertexAttributes ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    std::tie(outBlas->indexBuffer, indexBufferMem ) = createBuffer(
        indexData.size() * sizeof( uint32 ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    std::tie(outBlas->cumulativeTriangleAreaBuffer, cumulativeTriangleAreaMem) = createBuffer(
        cumulativeTriangleAreaData.size() * sizeof(float),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    outBlas->materialBuffer = params.material._buffer._buffer;

    auto [geoTransformBuffer, geoTransformBufferMem] = createBuffer(
        sizeof( Mat3x4 ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    void* dst;

    vkMapMemory( device, vertexPositionBufferMem, 0, positionData.size() * sizeof( VertexPosition ), 0, &dst );
    memcpy( dst, positionData.data(), positionData.size() * sizeof( VertexPosition ));
    vkUnmapMemory( device, vertexPositionBufferMem );

    vkMapMemory( device, vertexAttributeBufferMem, 0, attributeData.size() * sizeof( VertexAttributes ), 0, &dst );
    memcpy( dst, attributeData.data(), attributeData.size() * sizeof( VertexAttributes ) );
    vkUnmapMemory( device, vertexAttributeBufferMem );

    vkMapMemory( device, indexBufferMem, 0, indexData.size() * sizeof( uint32 ), 0, &dst );
    memcpy( dst, indexData.data(), indexData.size() * sizeof( uint32 ) );
    vkUnmapMemory( device, indexBufferMem );

    vkMapMemory(device, cumulativeTriangleAreaMem, 0, cumulativeTriangleAreaData.size() * sizeof(float), 0, &dst);
    memcpy(dst, cumulativeTriangleAreaData.data(), cumulativeTriangleAreaData.size() * sizeof(float));
    vkUnmapMemory(device, cumulativeTriangleAreaMem);

    vkMapMemory( device, geoTransformBufferMem, 0, sizeof( Mat3x4 ), 0, &dst );
    memcpy( dst, &transformData, sizeof( Mat3x4 ) );
    vkUnmapMemory( device, geoTransformBufferMem );

    VkAccelerationStructureGeometryKHR geometry0{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {.deviceAddress = getDeviceAddressOf(outBlas->vertexPositionBuffer ) },
                .vertexStride = sizeof( VertexPosition ),
				.maxVertex = ( uint32 )positionData.size() - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = getDeviceAddressOf(outBlas->indexBuffer ) },
                .transformData = {.deviceAddress = getDeviceAddressOf( geoTransformBuffer ) },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };
    VkAccelerationStructureGeometryKHR geometries[] = { geometry0 };

    uint32 triangleCount0 = indexData.size() / 3;
    uint32 triangleCounts[] = { triangleCount0 };

    VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = geometries,
    };

    VkAccelerationStructureBuildSizesInfoKHR requiredSize{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildBlasInfo,
        triangleCounts,
        &requiredSize );

    std::tie( outBlas->descriptor, outBlas->memory ) = createBuffer(
        requiredSize.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    auto [scratchBuffer, scratchBufferMem] = createBuffer(
        requiredSize.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    // Generate BLAS handle
    {
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = outBlas->descriptor,
            .size = requiredSize.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        };
        vkCreateAccelerationStructureKHR( device, &asCreateInfo, nullptr, &outBlas->handle );
    }

    // Build BLAS using GPU operations
    {
        vkResetCommandBuffer( commandBuffers[ imageIndex ], 0 );
        vkBeginCommandBuffer( commandBuffers[ imageIndex ], &beginInfo );
        {
            buildBlasInfo.dstAccelerationStructure = outBlas->handle;
            buildBlasInfo.scratchData.deviceAddress = getDeviceAddressOf( scratchBuffer );
            VkAccelerationStructureBuildRangeInfoKHR buildBlasRangeInfo[] =
            {
                {
                    .primitiveCount = triangleCounts[ 0 ],
                    .transformOffset = 0,
                }
            };

            VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfos[] = { buildBlasInfo };
            VkAccelerationStructureBuildRangeInfoKHR* buildBlasRangeInfos[] = { buildBlasRangeInfo };
            vkCmdBuildAccelerationStructuresKHR( commandBuffers[ imageIndex ], 1, buildBlasInfos, buildBlasRangeInfos );
        }
        vkEndCommandBuffer( commandBuffers[ imageIndex ] );

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[ imageIndex ],
        };
        vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( graphicsQueue );
    }

    vkFreeMemory( device, scratchBufferMem, nullptr );
    vkFreeMemory( device, geoTransformBufferMem, nullptr );
    vkDestroyBuffer( device, scratchBuffer, nullptr );
    vkDestroyBuffer( device, geoTransformBuffer, nullptr );

    return IAccelerationStructureRef( outBlas );
}

struct ObjectDesc
{
	uint64 vertexPositionDeviceAddress = 0;
	uint64 vertexAttributeDeviceAddress = 0;
	uint64 indexDeviceAddress = 0;
    uint64 cumulativeTriangleAreaAddress = 0;
    uint64 materialAddress = 0;
};

// @TODO: Support more than 1 instance
void VulkanRenderBackend::createTLAS( const std::vector<BLASBatch*>& batches )
{
    std::vector<VkAccelerationStructureInstanceKHR> instanceData;
    void* dst;

	size_t objectBufferCount = 0;
    for (int32 batchIndex = 0; batchIndex < batches.size(); ++batchIndex)
    {
        BLASBatch* batch = batches[batchIndex];
        objectBufferCount += batch->transforms.size();
    }
    const uint64 objectDescBufferSize = objectBufferCount * sizeof(ObjectDesc);
    VkDeviceMemory objectBufferMem;
    std::tie(objectBuffer, objectBufferMem) = createBuffer(
        objectDescBufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkMapMemory(device, objectBufferMem, 0, objectDescBufferSize, 0, &dst);
    for( int32 batchIndex = 0, objectIndex = 0; batchIndex < batches.size(); ++batchIndex )
    {
        BLASBatch* batch = batches[ batchIndex ];
        VulkanAccelerationStructure* blas = static_cast<VulkanAccelerationStructure*>( batch->blas.get() );

        VkAccelerationStructureInstanceKHR instance{};
        instance.mask = 0xFF;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = getDeviceAddressOf( blas->handle );

        for( int32 instanceIndex = 0; instanceIndex < batch->transforms.size(); ++instanceIndex, ++objectIndex)
        {
            const Mat3x4& world = toMat3x4(batch->transforms[instanceIndex]);
            memcpy( &instance, &batch->transforms[ instanceIndex ], sizeof( Mat3x4 )); // VkAccelerationStructureInstanceKHR::transform
            instance.instanceCustomIndex = objectIndex;
            instance.instanceShaderBindingTableRecordOffset = instanceData.size();

            instanceData.push_back( instance );
            ObjectDesc objectDesc
            {
				.vertexPositionDeviceAddress = getDeviceAddressOf(blas->vertexPositionBuffer),
				.vertexAttributeDeviceAddress = getDeviceAddressOf(blas->vertexAttributeBuffer),
				.indexDeviceAddress = getDeviceAddressOf(blas->indexBuffer),
                .cumulativeTriangleAreaAddress = getDeviceAddressOf(blas->cumulativeTriangleAreaBuffer),
                .materialAddress = getDeviceAddressOf(blas->materialBuffer)
            };
            memcpy((ObjectDesc*)dst + objectIndex, &objectDesc, sizeof(ObjectDesc));
        }
    }
    vkUnmapMemory(device, objectBufferMem);

    const int64 instanceDataByteSize = instanceData.size() * sizeof( VkAccelerationStructureInstanceKHR );

    auto [instanceBuffer, instanceBufferMem] = createBuffer(
        instanceDataByteSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    vkMapMemory( device, instanceBufferMem, 0, instanceDataByteSize, 0, &dst );
    memcpy( dst, instanceData.data(), instanceDataByteSize );
    vkUnmapMemory( device, instanceBufferMem );

    VkAccelerationStructureGeometryKHR instances{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = {.deviceAddress = getDeviceAddressOf( instanceBuffer ) },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    uint32_t instanceCount = instanceData.size();

    VkAccelerationStructureBuildGeometryInfoKHR buildTlasInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,     // It must be 1 with .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR as shown in the vulkan spec.
        .pGeometries = &instances,
    };

    VkAccelerationStructureBuildSizesInfoKHR requiredSize{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildTlasInfo,
        &instanceCount,
        &requiredSize );

    std::tie( tlasBuffer, tlasBufferMem ) = createBuffer(
        requiredSize.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    auto [scratchBuffer, scratchBufferMem] = createBuffer(
        requiredSize.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    // Generate TLAS handle
    {
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = tlasBuffer,
            .size = requiredSize.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        };
        vkCreateAccelerationStructureKHR( device, &asCreateInfo, nullptr, &tlas );
    }

    // Build TLAS using GPU operations
    {
        vkResetCommandBuffer( commandBuffers[ imageIndex ], 0 );
        vkBeginCommandBuffer( commandBuffers[ imageIndex ], &beginInfo );
        {
            buildTlasInfo.dstAccelerationStructure = tlas;
            buildTlasInfo.scratchData.deviceAddress = getDeviceAddressOf( scratchBuffer );

            VkAccelerationStructureBuildRangeInfoKHR buildTlasRangeInfo = { .primitiveCount = instanceCount };
            VkAccelerationStructureBuildRangeInfoKHR* buildTlasRangeInfo_[] = { &buildTlasRangeInfo };
            vkCmdBuildAccelerationStructuresKHR( commandBuffers[ imageIndex ], 1, &buildTlasInfo, buildTlasRangeInfo_ );
        }
        vkEndCommandBuffer( commandBuffers[ imageIndex ] );

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[ imageIndex ],
        };
        vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( graphicsQueue );
    }

    vkFreeMemory( device, scratchBufferMem, nullptr );
    vkFreeMemory( device, instanceBufferMem, nullptr );
    vkDestroyBuffer( device, scratchBuffer, nullptr );
    vkDestroyBuffer( device, instanceBuffer, nullptr );
}

void VulkanRenderBackend::createOutImage()
{
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM; // Back to 8-bit for display
    std::tie( outImage, outImageMem ) = createImage(
        { RenderSettings::screenWidth, RenderSettings::screenHeight },
        format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    VkImageViewCreateInfo ci0{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = outImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {},
            /*.r = VK_COMPONENT_SWIZZLE_B,
            .b = VK_COMPONENT_SWIZZLE_R,
        },*/
        .subresourceRange = subresourceRange,
    };
    vkCreateImageView( device, &ci0, nullptr, &outImageView );

    vkResetCommandBuffer( commandBuffers[ imageIndex ], 0 );
    vkBeginCommandBuffer( commandBuffers[ imageIndex ], &beginInfo );
    {
        setImageLayout(
            commandBuffers[ imageIndex ],
            outImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange );
    }
    vkEndCommandBuffer( commandBuffers[ imageIndex ] );

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[ imageIndex ],
    };
    vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( graphicsQueue );
}

void VulkanRenderBackend::createAccumulationImage()
{
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT; // High precision format
    std::tie( accumulationImage, accumulationImageMem ) = createImage(
        { RenderSettings::screenWidth, RenderSettings::screenHeight },
        format,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    VkImageViewCreateInfo ci0{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = accumulationImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {},
        .subresourceRange = subresourceRange,
    };
    vkCreateImageView( device, &ci0, nullptr, &accumulationImageView );

    vkResetCommandBuffer( commandBuffers[ imageIndex ], 0 );
    vkBeginCommandBuffer( commandBuffers[ imageIndex ], &beginInfo );
    {
        setImageLayout(
            commandBuffers[ imageIndex ],
            accumulationImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange );
    }
    vkEndCommandBuffer( commandBuffers[ imageIndex ] );

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffers[ imageIndex ],
    };
    vkQueueSubmit( graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( graphicsQueue );
}

#include "CameraObject.h"
#include "Scene.h"
void VulkanRenderBackend::createUniformBuffer()
{
    {
        struct alignas(16) Data
        {
            alignas(16) Vec3 camPos;
            alignas(16) Vec3 camFront;

            float yFov_degree{ };
            float exposure{ };
            uint32 frameCount{ };
            uint32 padding{ };
        } dataSrc;

        auto camera = tempScenePointer->getCamera();

        dataSrc = {
            .camPos   = camera->getWorldPosition(),
            .camFront = camera->getFront(),

            .yFov_degree = camera->getFov(),
            .exposure    = camera->getExposure(),
            .frameCount  = currentFrameCount
        };

        std::tie(cameraBuffer, cameraBufferMem) = createBuffer(
            sizeof(dataSrc),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* dst;
        vkMapMemory(device, cameraBufferMem, 0, sizeof(dataSrc), 0, &dst);
        *(Data*)dst = dataSrc;
        vkUnmapMemory(device, cameraBufferMem);
    }

    {
        imguiParam dataSrc = *tempScenePointer->getImguiParam();

        std::tie(imguiBuffer, imguiBufferMem) = createBuffer(
            sizeof(dataSrc),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* dst;
        vkMapMemory(device, imguiBufferMem, 0, sizeof(dataSrc), 0, &dst);
        memcpy(dst, &dataSrc, sizeof(imguiParam));
        vkUnmapMemory(device, imguiBufferMem);
    }
}

void VulkanRenderBackend::createLightBuffer()
{
    struct LightHeaderData
    {
        uint32 lightIdx[RenderSettings::maxLightCounts]; // 16 lights max
        uint32 lightCount;
        uint32 pad1;
        uint32 pad2;
        uint32 pad3;
        // LightData array follows
    };

    // Create an initial light buffer with space for up to 16 lights
    const size_t maxLights = RenderSettings::maxLightCounts;
    const size_t lightDataSize = sizeof(LightData);
    const size_t bufferSize = sizeof(LightHeaderData) + lightDataSize * maxLights; // header + lights

    std::tie( lightBuffer, lightBufferMem ) = createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    // Initialize with zero lights
    void* dst;
    vkMapMemory( device, lightBufferMem, 0, bufferSize, 0, &dst );
    memset( dst, 0, bufferSize );
    vkUnmapMemory( device, lightBufferMem );
}

void VulkanRenderBackend::updateLightBuffer( const std::vector<LightData>& lights )
{
    auto& lightIndex = tempScenePointer->getLightIndex();

    struct LightHeaderData
    {
        uint32 lightIdx[RenderSettings::maxLightCounts]; // 16 lights max
        uint32 lightCount;
        uint32 pad1;
        uint32 pad2;
        uint32 pad3;
        // LightData array follows
    };

    const size_t headerSize = sizeof(LightHeaderData);
    const size_t lightSize = sizeof(LightData) * lights.size();
    const size_t bufferSize = headerSize + lightSize;

    void* dst;
    vkMapMemory( device, lightBufferMem, 0, bufferSize, 0, &dst );

    // Write header
    LightHeaderData* header = (LightHeaderData*)dst;
    for (int i = 0; i < lights.size(); ++i)
        header->lightIdx[i] = lightIndex[i];
    header->lightCount = static_cast<uint32>(lights.size());
    header->pad1 = 0;
    header->pad2 = 0;
    header->pad3 = 0;

    // Write light data
    LightData* dstLights = (LightData*)(static_cast<uint8_t*>(dst) + headerSize);

    std::memcpy(dstLights, lights.data(), lightSize);

    vkUnmapMemory( device, lightBufferMem );
}

void VulkanRenderBackend::updateCameraBuffer()
{
    {
        struct alignas(16) Data
        {
            alignas(16) Vec3 camPos;
            alignas(16) Vec3 camFront;

            float yFov_degree{ };
            float exposure{ };
            uint32 frameCount{ };
            uint32 padding{ };
        } dataSrc;

        auto camera = tempScenePointer->getCamera();

        dataSrc = {
            .camPos   = camera->getWorldPosition(),
            .camFront = camera->getFront(),

            .yFov_degree = camera->getFov(),
            .exposure    = camera->getExposure(),
            .frameCount  = currentFrameCount
        };

        void* dst;
        vkMapMemory(device, cameraBufferMem, 0, sizeof(dataSrc), 0, &dst);
        *(Data*)dst = dataSrc;
        vkUnmapMemory(device, cameraBufferMem);
    }
}

void A3::VulkanRenderBackend::updateImguiBuffer()
{
    {
        imguiParam dataSrc = *tempScenePointer->getImguiParam();

        void* dst;
        vkMapMemory(device, imguiBufferMem, 0, sizeof(dataSrc), 0, &dst);
        memcpy(dst, &dataSrc, sizeof(imguiParam));
        vkUnmapMemory(device, imguiBufferMem);
    }
}

VkShaderStageFlagBits getVulkanShaderStage( EShaderStage stage )
{
    switch( stage )
    {
        case SS_Vertex:         return VK_SHADER_STAGE_VERTEX_BIT;
        case SS_Fragment:       return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SS_Compute:        return VK_SHADER_STAGE_COMPUTE_BIT;
        case SS_RayGeneration:  return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case SS_AnyHit:         return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case SS_ClosestHit:     return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case SS_Miss:           return VK_SHADER_STAGE_MISS_BIT_KHR;
    }

    // Should not reach here
    return VK_SHADER_STAGE_ALL;
}

VkRayTracingShaderGroupTypeKHR getVulkanShaderGroup( EShaderStage stage )
{
    switch( stage )
    {
        case SS_AnyHit:
        case SS_ClosestHit:
            return VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    }

    return VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
}

VkDescriptorType getVulkanShaderDescriptorType( EShaderResourceDescriptor type )
{
    switch( type )
    {
        case SRD_AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        case SRD_StorageBuffer:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SRD_StorageImage:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SRD_UniformBuffer:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SRD_Sampler:               return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SRD_ImageSampler:          return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }

    // Should not reach here
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

bool isVulkanClosestHitShader( EShaderStage stage ) { return stage == SS_ClosestHit; }
bool isVulkanAnyHitShader( EShaderStage stage ) { return stage == SS_AnyHit; }
bool isVulkanIntersectionShader( EShaderStage stage ) { return stage == SS_Intersection; }
bool isVulkanGeneralShader( EShaderStage stage ) { return !isVulkanClosestHitShader( stage ) && !isVulkanAnyHitShader( stage ) && !isVulkanIntersectionShader( stage ); }

IRenderPipelineRef VulkanRenderBackend::createRayTracingPipeline( const RaytracingPSODesc& psoDesc, RaytracingPSO* pso )
{
    VulkanPipeline* outPipeline = new VulkanPipeline();

    //==========================================================
    // Pipeline layout
    //==========================================================
    std::vector<VkDescriptorSetLayoutBinding> bindings( 12 );
    for( const ShaderDesc& shaderDesc : psoDesc.shaders )
    {
        for( const ShaderResourceDescriptor& descriptor : shaderDesc.descriptors )
        {
            VkDescriptorSetLayoutBinding& binding = bindings[ descriptor.index ];
            binding.binding = descriptor.index;
            binding.descriptorType = getVulkanShaderDescriptorType( descriptor.type );
            binding.descriptorCount = 1;
            binding.stageFlags |= getVulkanShaderStage( shaderDesc.type );
        }
    }

    VkDescriptorSetLayoutBinding textureBindlessBinding = {};
    textureBindlessBinding.binding = TEXTUREBINDLESS_BINDING_LOCATION;
    textureBindlessBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureBindlessBinding.descriptorCount = TEXTUREBINDLESS_BINDING_MAX_COUNT;
    textureBindlessBinding.stageFlags = getVulkanShaderStage(EShaderStage::SS_ClosestHit);
    bindings[bindings.size() - 1] = textureBindlessBinding;
    
    VkDescriptorBindingFlags textureBindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;// VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;// | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), 0);
    bindingFlags[bindingFlags.size() - 1] = textureBindingFlags;

    VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{};
    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    extendedInfo.bindingCount = bindingFlags.size();
    extendedInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateFlags flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &extendedInfo, 
        .flags = flags, 
        .bindingCount = ( uint32 )bindings.size(),
        .pBindings = bindings.data(),
    };
    VkResult result = vkCreateDescriptorSetLayout( device, &descriptorLayoutCreateInfo, nullptr, &outPipeline->descriptorSetLayout );

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &outPipeline->descriptorSetLayout,
    };
    vkCreatePipelineLayout( device, &pipelineLayoutCreateInfo, nullptr, &outPipeline->pipelineLayout );

    //==========================================================
    // Pipeline
    //==========================================================
    std::vector<VkPipelineShaderStageCreateInfo> stages( psoDesc.shaders.size() );
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    int closestHitStage = -1, anyHitStage = -1;

    for( uint32 index = 0; index < stages.size(); ++index )
    {
        VulkanShaderModule* vkModule = static_cast< VulkanShaderModule* >( pso->shaders[ index ] );
        const ShaderDesc& desc = psoDesc.shaders[ index ];

        stages[ index ] = VkPipelineShaderStageCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = getVulkanShaderStage( desc.type ),
            .module = vkModule->module,
            .pName = "main",
        };

        if (desc.type == SS_ClosestHit) { closestHitStage = index; continue; }
        if (desc.type == SS_AnyHit) { anyHitStage = index; continue; }

        VkRayTracingShaderGroupCreateInfoKHR hitGroup{
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = getVulkanShaderGroup(desc.type), // VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR
            .generalShader = index,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        };
        groups.push_back(hitGroup);
    }
    {
        VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
        hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
        hitGroup.closestHitShader = closestHitStage;
        hitGroup.anyHitShader = anyHitStage;
        hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(hitGroup);
    }

    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = ( uint32 )stages.size(),
        .pStages = stages.data(),
        .groupCount = ( uint32 )groups.size(),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 31,
        .layout = outPipeline->pipelineLayout,
    };
    vkCreateRayTracingPipelinesKHR( device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &outPipeline->pipeline );

    //==========================================================
    // Descriptor set
    //==========================================================
	VkDescriptorSetAllocateInfo allocateInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &outPipeline->descriptorSetLayout,
	};
    result = vkAllocateDescriptorSets( device, &allocateInfo, &outPipeline->descriptorSet );

    {
        struct ScopedWriteDescriptorSets
        {
            std::vector<VkWriteDescriptorSet> descriptors;

            std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructures;
            std::vector<VkDescriptorImageInfo> images;
            std::vector<VkDescriptorBufferInfo> buffers;

            std::vector<VkDescriptorImageInfo> texture_image_infos;
            std::vector<VkDescriptorImageInfo> sampler_infos;
        };

        ScopedWriteDescriptorSets writeDescriptorSets;

        { // @TODO: Deterministic resize
            writeDescriptorSets.descriptors.resize( bindings.size() );
            writeDescriptorSets.accelerationStructures.reserve( bindings.size() );
            writeDescriptorSets.images.reserve( bindings.size() );
            writeDescriptorSets.buffers.reserve( bindings.size() );
            writeDescriptorSets.texture_image_infos.reserve(TextureManager::gTextureArray.size());
            writeDescriptorSets.sampler_infos.reserve(bindings.size());
        }

        // @TODO: Move to scene level
        std::vector<VkBuffer> storageBuffers =
        {
            nullptr, nullptr,
            cameraBuffer, objectBuffer,
            lightBuffer, nullptr, nullptr, imguiBuffer
        };

        std::vector<VkWriteDescriptorSet> validDescriptors;

        for( int32 index = 0; index < bindings.size(); ++index )
        {
            const VkDescriptorSetLayoutBinding& binding = bindings[ index ];

            VkWriteDescriptorSet descriptor{};
            descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor.dstSet = outPipeline->descriptorSet;
            descriptor.descriptorCount = 1;
            descriptor.dstBinding = index;
            descriptor.descriptorType = binding.descriptorType;

            if( binding.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR )
            {
                writeDescriptorSets.accelerationStructures.emplace_back(
                    VkWriteDescriptorSetAccelerationStructureKHR
                    {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                        .accelerationStructureCount = 1,
                        .pAccelerationStructures = &tlas
                    }
                );

                descriptor.pNext = &writeDescriptorSets.accelerationStructures.back();
            }
            else if( binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE )
            {
                // binding 1 is output image, binding 5 is accumulation image
                VkImageView imageView = (index == 1) ? outImageView : accumulationImageView;

                writeDescriptorSets.images.emplace_back(
                    VkDescriptorImageInfo
                    {
                        .imageView = imageView,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                    }
                );

                descriptor.pImageInfo = &writeDescriptorSets.images.back();
            }
            else if( binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
            {
                VkBuffer buffer = storageBuffers[ index ];
                if (buffer == nullptr) {
                    printf("WARNING: Storage buffer at index %d is null (binding %d)\n", index, binding.binding);
                    // Skip this descriptor for now
                    continue;
                }

                writeDescriptorSets.buffers.emplace_back(
                    VkDescriptorBufferInfo
                    {
                        .buffer = buffer,
                        .offset = 0,
                        .range = VK_WHOLE_SIZE
                    }
                );

                descriptor.pBufferInfo = &writeDescriptorSets.buffers.back();
            }
            else if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            {
                writeDescriptorSets.images.emplace_back(
                    VkDescriptorImageInfo{
                        .sampler = envSampler,
                        .imageView = index == 6 ? envImageView : index == 8 ? envImportanceView : envHitView, // @TODO: decouple index based logic
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    }
                );

                descriptor.pImageInfo = &writeDescriptorSets.images.back();
            }
            
            validDescriptors.push_back(descriptor);
        }

        if (!validDescriptors.empty()) {
            vkUpdateDescriptorSets( device, validDescriptors.size(), validDescriptors.data(), 0, VK_NULL_HANDLE );
        }
    }

    //==========================================================
    // Shader binding table
    //==========================================================
    struct ShaderGroupHandle
    {
        uint8 data[ RenderSettings::shaderGroupHandleSize ];
    };

    struct HitgCustomData
    {
        float color[ 3 ];
        float metallic;
        float roughness;
    };

    auto alignTo = []( auto value, auto alignment ) -> decltype( value )
        {
            return ( value + ( decltype( value ) )alignment - 1 ) & ~( ( decltype( value ) )alignment - 1 );
        };
    const uint32 handleSize = RenderSettings::shaderGroupHandleSize;
    const uint32 groupCount = static_cast<uint32>( groups.size() ); // 1 raygen, 2 miss, 1 hit group
    std::vector<ShaderGroupHandle> handles( groupCount );
    vkGetRayTracingShaderGroupHandlesKHR( device, outPipeline->pipeline,
                                          0, groupCount,
                                          handleSize*groupCount,
                                          handles.data() );
    ShaderGroupHandle rgenHandle = handles[ 0 ];
    ShaderGroupHandle missHandle = handles[ 1 ];
    ShaderGroupHandle shadowMissHandle = handles[ 2 ];
    ShaderGroupHandle hitgHandle = handles[ 3 ];

    const uint32 rgenStride = alignTo( handleSize, rtProperties.shaderGroupHandleAlignment );
    rgenSbt = { 0, rgenStride, rgenStride };

    const uint64 missOffset = alignTo( rgenSbt.size, rtProperties.shaderGroupBaseAlignment );
    const uint32 missStride = alignTo( handleSize, rtProperties.shaderGroupHandleAlignment );
    missSbt = { 0, missStride, missStride * 2 };

    std::vector<MeshObject*> objects = tempScenePointer->collectMeshObjects();
    const uint32 hitgCustomDataSize = sizeof( HitgCustomData );
    const uint32 geometryCount = objects.size();
    const uint64 hitgOffset = alignTo( missOffset + missSbt.size, rtProperties.shaderGroupBaseAlignment );
    const uint32 hitgStride = alignTo( handleSize + hitgCustomDataSize, rtProperties.shaderGroupHandleAlignment );
    hitgSbt = { 0, hitgStride, hitgStride * geometryCount };

    const uint64 sbtSize = hitgOffset + hitgSbt.size;
    std::tie( sbtBuffer, sbtBufferMem ) = createBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    auto sbtAddress = getDeviceAddressOf( sbtBuffer );
    if( sbtAddress != alignTo( sbtAddress, rtProperties.shaderGroupBaseAlignment ) )
    {
        throw std::runtime_error( "It will not be happened!" );
    }
    rgenSbt.deviceAddress = sbtAddress;
    missSbt.deviceAddress = sbtAddress + missOffset;
    hitgSbt.deviceAddress = sbtAddress + hitgOffset;

    uint8* dst;
    vkMapMemory( device, sbtBufferMem, 0, sbtSize, 0, ( void** )&dst );
    {
        *( ShaderGroupHandle* )dst = rgenHandle;
        *( ShaderGroupHandle* )( dst + missOffset + 0 * missStride ) = missHandle;
        *( ShaderGroupHandle* )( dst + missOffset + 1 * missStride ) = shadowMissHandle;

        for (size_t i = 0; i < geometryCount; ++i)
        {
            HitgCustomData mat;
            const auto& objectColor = objects[i]->getBaseColor();
            const auto& objectMetallic = objects[i]->getMetallic();
            const auto& objectRoughness = objects[i]->getRoughness();
            mat = { objectColor.x, objectColor.y, objectColor.z, objectMetallic, objectRoughness };
            *(ShaderGroupHandle*)(dst + hitgOffset + i * hitgStride) = hitgHandle;
            *(HitgCustomData*)(dst + hitgOffset + i * hitgStride + handleSize) = mat;
        }
    }
    vkUnmapMemory( device, sbtBufferMem );

    return IRenderPipelineRef( outPipeline );
}

/*
In the vulkan spec,
[VUID-vkCmdTraceRaysKHR-stride-03686] pMissShaderBindingTable->stride must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleAlignment
[VUID-vkCmdTraceRaysKHR-stride-03690] pHitShaderBindingTable->stride must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleAlignment
[VUID-vkCmdTraceRaysKHR-pRayGenShaderBindingTable-03682] pRayGenShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment
[VUID-vkCmdTraceRaysKHR-pMissShaderBindingTable-03685] pMissShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment
[VUID-vkCmdTraceRaysKHR-pHitShaderBindingTable-03689] pHitShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment

As shown in the vulkan spec 40.3.1. Indexing Rules,
    pHitShaderBindingTable->deviceAddress + pHitShaderBindingTable->stride (
    instanceShaderBindingTableRecordOffset + geometryIndex ??sbtRecordStride + sbtRecordOffset )
*/

void VulkanRenderBackend::saveCurrentImage(const std::string& filename)
{
    // Wait for rendering to complete
    vkDeviceWaitIdle(device);

    // Get image dimensions
    uint32_t width = swapChainImageExtent.width;
    uint32_t height = swapChainImageExtent.height;

    // Create staging buffer for image data
    VkDeviceSize imageSize = width * height * 4; // RGBA8
    auto [stagingBuffer, stagingBufferMem] = createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Create command buffer for copy operation
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPools[imageIndex];
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Transition image layout for transfer
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = outImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(
        cmdBuffer,
        outImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer,
        1,
        &region
    );

    // Transition back to general layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPools[imageIndex], 1, &cmdBuffer);

    // Map buffer and save to file
    void* data;
    vkMapMemory(device, stagingBufferMem, 0, imageSize, 0, &data);

    // Convert BGRA to RGBA for stb_image_write
    uint8_t* pixels = (uint8_t*)data;
    std::vector<uint8_t> rgbaData(width * height * 4);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = (y * width + x) * 4;
            uint32_t outIdx = (y * width + x) * 4;
            // Convert BGRA to RGBA
            rgbaData[outIdx + 0] = pixels[idx + 2]; // R (from B position)
            rgbaData[outIdx + 1] = pixels[idx + 1]; // G
            rgbaData[outIdx + 2] = pixels[idx + 0]; // B (from R position)
            rgbaData[outIdx + 3] = pixels[idx + 3]; // A
        }
    }

    // Save as PNG
    std::string folder = "output_images";
    std::string path = folder + "/" + filename;

    if (!std::filesystem::exists(folder))
        std::filesystem::create_directories(folder);

    int result = stbi_write_png(path.c_str(), width, height, 4, rgbaData.data(), width * 4);
    if (result) {
        printf("Image saved as: %s\n", path.c_str());
    } else {
        printf("Failed to save image: %s\n", path.c_str());
    }

    vkUnmapMemory(device, stagingBufferMem);

    // Cleanup
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMem, nullptr);
}

const A3Buffer A3::VulkanRenderBackend::createResourceBuffer(const uint32 size, const void* data)
{
    A3Buffer buffer;

    if (size <= 0)
        return buffer;

    std::tie(buffer._buffer, buffer._memory) = createBuffer(
        size,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dst;
    vkMapMemory(device, buffer._memory, 0, size, 0, &dst);
    memcpy(dst, data, size);
    vkUnmapMemory(device, buffer._memory);

    return buffer;
}

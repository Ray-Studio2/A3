#include "Vulkan.h"
//#include "shader_module.h"
#include "RenderSettings.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_glfw.h"
#include "ThirdParty/imgui/imgui_impl_vulkan.h"
#include "MeshResource.h"
#include "VulkanResource.h"
#include "AccelerationStructure.h"
#include "Shader.h"
#include "PipelineStateObject.h"
#include <random>

using namespace A3;

VulkanRenderBackend::VulkanRenderBackend( GLFWwindow* window, std::vector<const char*>& extensions, int32 screenWidth, int32 screenHeight )
{
    createVkInstance( extensions );
    createVkPhysicalDevice();
    createVkSurface( window );
    createVkQueueFamily();
    createVkDescriptorPools();
    createSwapChain();
    createImguiRenderPass( screenWidth, screenHeight );
    createCommandCenter();
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

void VulkanRenderBackend::beginFrame( int32 screenWidth, int32 screenHeight )
{
    VkSemaphore image_acquired_semaphore = imageAvailableSemaphores[ semaphoreIndex ];
    VkSemaphore render_complete_semaphore = renderFinishedSemaphores[ semaphoreIndex ];
    VkResult err = vkAcquireNextImageKHR( device, swapChain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &imageIndex );

    {
        err = vkWaitForFences( device, 1, &fences[ imageIndex ], VK_TRUE, UINT64_MAX );    // wait indefinitely instead of periodically checking
        check_vk_result( err );

        err = vkResetFences( device, 1, &fences[ imageIndex ] );
        check_vk_result( err );
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

    vkQueuePresentKHR( graphicsQueue, &presentInfo );

    semaphoreIndex = ( semaphoreIndex + 1 ) % 3;
}

void VulkanRenderBackend::beginRaytracingPipeline( IRenderPipeline* inPipeline )
{
    VulkanPipeline* pipeline = static_cast< VulkanPipeline* >( inPipeline );

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
        .pSignalSemaphores = &renderFinishedSemaphores[ semaphoreIndex ],
    };

    vkEndCommandBuffer( commandBuffers[ imageIndex ] );
    vkQueueSubmit( graphicsQueue, 1, &submitInfo, fences[ imageIndex ] );

    {
        VkResult err;
        err = vkWaitForFences( device, 1, &fences[ imageIndex ], VK_TRUE, UINT64_MAX );    // wait indefinitely instead of periodically checking
        check_vk_result( err );

        err = vkResetFences( device, 1, &fences[ imageIndex ] );
        check_vk_result( err );
    }
}

void VulkanRenderBackend::rebuildAccelerationStructure()
{
    createOutImage();
    createUniformBuffer();
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
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
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
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // not used
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,

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

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = ( uint32 )deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures f1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR f2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE,
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR f3{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE,
    };

    createInfo.pNext = &f1;
    f1.pNext = &f2;
    f2.pNext = &f3;

    if( vkCreateDevice( physicalDevice, &createInfo, nullptr, &device ) != VK_SUCCESS )
    {
        throw std::runtime_error( "failed to create logical device!" );
    }

    vkGetDeviceQueue( device, queueFamilyIndex, 0, &graphicsQueue );

    // @TODO: Isolate function
    loadDeviceExtensionFunctions( device );
}

void VulkanRenderBackend::createVkDescriptorPools()
{
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
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
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
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

    const VkColorSpaceKHR defaultSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
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
            vkCreateSemaphore( device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[ i ]) != VK_SUCCESS ||
            vkCreateFence( device, &fenceInfo, nullptr, &fences[ i ] ) != VK_SUCCESS )
        {
            throw std::runtime_error( "failed to create synchronization objects for a frame!" );
        }
    }
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
        VkMemoryAllocateFlagsInfo flagsInfo{
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
IAccelerationStructureRef VulkanRenderBackend::createBLAS(
    const std::vector<VertexPosition>& positionData, 
    const std::vector<VertexAttributes>& attributeData, 
    const std::vector<uint32>& indexData, 
    const Mat3x4& transformData )
{
    VulkanAccelerationStructure* outBlas = new VulkanAccelerationStructure();

    std::tie( vertexPositionBuffer, vertexPositionBufferMem ) = createBuffer(
        positionData.size() * sizeof( VertexPosition ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    std::tie( vertexAttributeBuffer, vertexAttributeBufferMem ) = createBuffer(
        attributeData.size() * sizeof( VertexAttributes ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    std::tie( indexBuffer, indexBufferMem ) = createBuffer(
        indexData.size() * sizeof( uint32 ),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

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
                .vertexData = {.deviceAddress = getDeviceAddressOf( vertexPositionBuffer ) },
                .vertexStride = sizeof( VertexPosition ),
				.maxVertex = ( uint32 )positionData.size() - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = getDeviceAddressOf( indexBuffer ) },
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

// @TODO: Support more than 1 instance
void VulkanRenderBackend::createTLAS( const std::vector<BLASBatch*>& batches )
{
    std::vector<VkAccelerationStructureInstanceKHR> instanceData;

    for( int32 batchIndex = 0; batchIndex < batches.size(); ++batchIndex )
    {
        BLASBatch* batch = batches[ batchIndex ];
        VulkanAccelerationStructure* blas = static_cast<VulkanAccelerationStructure*>( batch->blas.get() );

        VkAccelerationStructureInstanceKHR instance{};
        instance.instanceCustomIndex = 100;
        instance.mask = 0xFF;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = getDeviceAddressOf( blas->handle );

        for( int32 instanceIndex = 0; instanceIndex < batch->transforms.size(); ++instanceIndex )
        {
            instance.instanceShaderBindingTableRecordOffset = instanceData.size();
            memcpy( &instance, &batch->transforms[ instanceIndex ], sizeof( Mat3x4 ) );

            instanceData.push_back( instance );
        }
    }

    const int64 instanceDataByteSize = instanceData.size() * sizeof( VkAccelerationStructureInstanceKHR );

    auto [instanceBuffer, instanceBufferMem] = createBuffer(
        instanceDataByteSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    void* dst;
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
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM; //VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB(==swapChainImageFormat)
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

#include "CameraObject.h"
#include "Scene.h"
void VulkanRenderBackend::createUniformBuffer()
{
    struct Data
    {
        float cameraPos[ 3 ];
        float yFov_degree;
    } dataSrc;

    std::tie( uniformBuffer, uniformBufferMem ) = createBuffer(
        sizeof( dataSrc ),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    CameraObject* co = tempScenePointer->getCamera();
    const Vec3& pos = co->getPosition();
    float cameraPos[3] = { pos.x, pos.y, pos.z };
    float fov = co->getFov();

    void* dst;
    vkMapMemory( device, uniformBufferMem, 0, sizeof( dataSrc ), 0, &dst );
    *( Data* )dst = { cameraPos[0], cameraPos[1], cameraPos[2], fov};
    vkUnmapMemory( device, uniformBufferMem );
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
    std::vector<VkDescriptorSetLayoutBinding> bindings( 6 );
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

    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ( uint32 )bindings.size(),
        .pBindings = bindings.data(),
    };
    vkCreateDescriptorSetLayout( device, &descriptorLayoutCreateInfo, nullptr, &outPipeline->descriptorSetLayout );

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
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups( psoDesc.shaders.size() );
    for( int32 index = 0; index < stages.size(); ++index )
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

        groups[ index ] = VkRayTracingShaderGroupCreateInfoKHR
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = getVulkanShaderGroup( desc.type ),
            .generalShader = isVulkanGeneralShader( desc.type ) ? index : VK_SHADER_UNUSED_KHR,
            .closestHitShader = isVulkanClosestHitShader( desc.type ) ? index : VK_SHADER_UNUSED_KHR,
            .anyHitShader = isVulkanAnyHitShader( desc.type ) ? index : VK_SHADER_UNUSED_KHR,
            .intersectionShader = isVulkanIntersectionShader( desc.type ) ? index : VK_SHADER_UNUSED_KHR,
        };
    }

    VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo
    {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = ( uint32 )stages.size(),
        .pStages = stages.data(),
        .groupCount = ( uint32 )groups.size(),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
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
    vkAllocateDescriptorSets( device, &allocateInfo, &outPipeline->descriptorSet );

    {
        struct ScopedWriteDescriptorSets
        {
            std::vector<VkWriteDescriptorSet> descriptors;

            std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructures;
            std::vector<VkDescriptorImageInfo> images;
            std::vector<VkDescriptorBufferInfo> buffers;
        };

        ScopedWriteDescriptorSets writeDescriptorSets;

        { // @TODO: Deterministic resize
            writeDescriptorSets.descriptors.resize( bindings.size() );
            writeDescriptorSets.accelerationStructures.reserve( bindings.size() );
            writeDescriptorSets.images.reserve( bindings.size() );
            writeDescriptorSets.buffers.reserve( bindings.size() );
        }

        // @TODO: Move to scene level
        std::vector<VkBuffer> storageBuffers = { nullptr, nullptr, uniformBuffer, vertexPositionBuffer, vertexAttributeBuffer, indexBuffer };

        for( int32 index = 0; index < bindings.size(); ++index )
        {
            const VkDescriptorSetLayoutBinding& binding = bindings[ index ];

            VkWriteDescriptorSet& descriptor = writeDescriptorSets.descriptors[ index ];
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
                writeDescriptorSets.images.emplace_back(
                    VkDescriptorImageInfo
                    {
                        .imageView = outImageView,
                        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
                    }
                );

                descriptor.pImageInfo = &writeDescriptorSets.images.back();
            }
            else if( binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
            {
                writeDescriptorSets.buffers.emplace_back(
                    VkDescriptorBufferInfo
                    {
                        .buffer = storageBuffers[ index ],
                        .offset = 0,
                        .range = VK_WHOLE_SIZE
                    }
                );

                descriptor.pBufferInfo = &writeDescriptorSets.buffers.back();
            }
        }

        vkUpdateDescriptorSets( device, writeDescriptorSets.descriptors.size(), writeDescriptorSets.descriptors.data(), 0, VK_NULL_HANDLE );
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
    };

    auto alignTo = []( auto value, auto alignment ) -> decltype( value )
        {
            return ( value + ( decltype( value ) )alignment - 1 ) & ~( ( decltype( value ) )alignment - 1 );
        };
    const uint32 handleSize = RenderSettings::shaderGroupHandleSize;
    const uint32 groupCount = 4; // 1 raygen, 2 miss, 1 hit group
    std::vector<ShaderGroupHandle> handles( groupCount );
    vkGetRayTracingShaderGroupHandlesKHR( device, outPipeline->pipeline, 0, groupCount, handleSize* groupCount, handles.data() );
    ShaderGroupHandle rgenHandle = handles[ 0 ];
    ShaderGroupHandle missHandle = handles[ 1 ];
    ShaderGroupHandle hitgHandle = handles[ 2 ];
    ShaderGroupHandle shadowMissHandle = handles[ 3 ];

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

        const HitgCustomData sampleColorTable[] =
        { 
            { 0.6f, 0.1f, 0.2f }  // Deep Red Wine
            , { 0.1f, 0.8f, 0.4f } // Emerald Green
            , { 0.9f, 0.7f, 0.1f } // Golden Yellow
            , { 0.3f, 0.6f, 0.9f } // Dawn Sky Blue
        };

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < geometryCount; ++i)
        {
            HitgCustomData color;
            if (i < IM_ARRAYSIZE(sampleColorTable))
            {
                color = sampleColorTable[i];
            }
            else
            {
                color.color[0] = dist(gen);
                color.color[1] = dist(gen);
				color.color[2] = dist(gen);
            }
            *(ShaderGroupHandle*)(dst + hitgOffset + i * hitgStride) = hitgHandle;
            *(HitgCustomData*)(dst + hitgOffset + i * hitgStride + handleSize) = color;
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
    pHitShaderBindingTable->deviceAddress + pHitShaderBindingTable->stride �� (
    instanceShaderBindingTableRecordOffset + geometryIndex �� sbtRecordStride + sbtRecordOffset )
*/

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <tuple>
#include <bitset>
#include <span>
#include <memory>
#include <string>
#include "EngineTypes.h"
#include "RenderSettings.h"
#include "RenderBackend.h"
#include "Matrix.h"

#ifdef NDEBUG
const bool ON_DEBUG = false;
#else
const bool ON_DEBUG = true;
#endif

namespace A3
{
struct VertexPosition;
struct VertexAttributes;
class VulkanRenderBackend;

struct TextureParameter
{
    static constexpr const unsigned int InvalidTextureParameter = 0xfffffff;
    uint32 _index = InvalidTextureParameter;
};
class TextureManager
{
public:
    struct TextureView
    {
        std::string _path;

        VkImage _image;
        VkDeviceMemory _memory;
        VkImageView _view;
    };

    static void initialize(VulkanRenderBackend& vulkanBackend);

    static const TextureParameter createTexture(VulkanRenderBackend& vulkanBackend, const std::string path, const uint32 imageFormat, const uint32 width, const uint32 height, const float* pixelData);

    static std::vector<TextureView> gTextureArray;
    static TextureParameter gWhiteParameter;
    static VkSampler gLinearSampler;
};

struct A3Buffer
{
    VkBuffer _buffer;
    VkDeviceMemory _memory;
};

struct MaterialParameter
{
    MaterialParameter();

    Vec4 _baseColorFactor;

    TextureParameter _baseColorTexture;
    TextureParameter _normalTexture;
    TextureParameter _occlusionTexture;
    float _metallicFactor;

    float _roughnessFactor;
    TextureParameter _metallicRoughnessTexture;
    float _padding1;
    float _padding2;

    Vec3 _emissiveFactor;
    TextureParameter _emissiveTexture;
};
struct Material
{
    MaterialParameter _parameter;

    // innerdata
    A3Buffer _buffer;
    std::string _name;

    void uploadMaterialParameter(VulkanRenderBackend& vulkanBackend);
};

class VulkanRenderBackend : public IRenderBackend
{
    friend struct Material;
    friend class TextureManager;

public:
    VulkanRenderBackend( GLFWwindow* window, std::vector<const char*>& extensions, int32 screenWidth, int32 screenHeight );
    ~VulkanRenderBackend();

    virtual void beginFrame( int32 screenWidth, int32 screenHeight ) override;
    virtual void endFrame() override;

    virtual void beginRaytracingPipeline( IRenderPipeline* inPipeline ) override;

    virtual void rebuildAccelerationStructure() override;

    //@TODO: Move to renderer
    const class Scene* tempScenePointer = nullptr;
    uint32 currentFrameCount = 0;
    virtual IAccelerationStructureRef createBLAS(const BLASBuildParams params) override;
    virtual void createTLAS( const std::vector<BLASBatch*>& batches ) override;
    virtual IShaderModuleRef createShaderModule( const ShaderDesc& desc ) override;
    virtual IRenderPipelineRef createRayTracingPipeline( const RaytracingPSODesc& psoDesc, RaytracingPSO* pso ) override;
    virtual void updateLightBuffer( const std::vector<LightData>& lights ) override;
    TextureManager::TextureView createResourceImage(const uint32 imageFormat, const uint32 width, const uint32 height, const float* pixelData);
    void createOutImage();
    void createAccumulationImage();
    void createUniformBuffer();
    void createLightBuffer();
    void updateCameraBuffer();
    void updateImguiBuffer();
    void saveCurrentImage(const std::string& filename);
    //////////////////////////

    const A3Buffer createResourceBuffer(const uint32 size, const void* data);

private:
    void createVkInstance( std::vector<const char*>& extensions );
    void createVkPhysicalDevice();
    void createVkSurface( GLFWwindow* window );
    void createVkQueueFamily();
    void createVkDescriptorPools();
    void createSwapChain();
    void createImguiRenderPass( int32 screenWidth, int32 screenHeight );
    void createCommandCenter();
    void createEnvironmentMap(std::string_view hdrTexturePath);
    void createEnvironmentMapImportanceSampling(float* pixels, int width, int height);

    void loadDeviceExtensionFunctions( VkDevice device );

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData );

    bool checkValidationLayerSupport( std::vector<const char*>& reqestNames );

    bool checkDeviceExtensionSupport( VkPhysicalDevice device, std::vector<const char*>& reqestNames );

    uint32 findMemoryType( uint32_t memoryTypeBits, VkMemoryPropertyFlags reqMemProps );

    std::tuple<VkBuffer, VkDeviceMemory> createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags reqMemProps );

    std::tuple<VkImage, VkDeviceMemory> createImage(
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags reqMemProps );

    void setImageLayout(
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT );

public:
    VkDeviceAddress getDeviceAddressOf( VkBuffer buffer );

    VkDeviceAddress getDeviceAddressOf( VkAccelerationStructureKHR as );

private:
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rtProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

    VkAllocationCallbacks* allocator;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    VkQueue graphicsQueue; // assume allowing graphics and present
    uint32 queueFamilyIndex;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> framebuffers;
    const VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;// VK_FORMAT_R16G16B16A16_SFLOAT;    // intentionally chosen to match a specific format
    const VkExtent2D swapChainImageExtent = { .width = RenderSettings::screenWidth, .height = RenderSettings::screenHeight };

    std::vector<VkCommandPool> commandPools;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> rtFinishedSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> fences;
    uint32 semaphoreIndex;
    uint32 imageIndex;

    VkBuffer tlasBuffer;
    VkDeviceMemory tlasBufferMem;
    VkAccelerationStructureKHR tlas;

    VkImage envImage;
    VkDeviceMemory envImageMem;
    VkImageView envImageView;
    VkSampler envSampler;

    VkImage envImportanceImage;
    VkDeviceMemory envImportanceMem;
    VkImageView envImportanceView;

    VkImage envHitImage;
    VkDeviceMemory envHitMem;
    VkImageView envHitView;

    VkImage outImage;
    VkDeviceMemory outImageMem;
    VkImageView outImageView;

    VkImage accumulationImage;
    VkDeviceMemory accumulationImageMem;
    VkImageView accumulationImageView;

    VkBuffer cameraBuffer;
    VkDeviceMemory cameraBufferMem;

    VkBuffer lightBuffer;
    VkDeviceMemory lightBufferMem;

    VkBuffer imguiBuffer;
    VkDeviceMemory imguiBufferMem;

    VkDescriptorPool descriptorPool;
    VkBuffer objectBuffer;

    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMem;
    VkStridedDeviceAddressRegionKHR rgenSbt{};
    VkStridedDeviceAddressRegionKHR missSbt{};
    VkStridedDeviceAddressRegionKHR hitgSbt{};
    VkStridedDeviceAddressRegionKHR callSbt{};

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    VkImageCopy copyRegion = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .extent = { RenderSettings::screenWidth, RenderSettings::screenHeight, 1 },
    };

    VkRenderPass imguiRenderPass;

    friend class Addon_imgui;
};
}
#pragma once

#include "EngineTypes.h"
#include "Scene.h"
#include <vector>

struct GLFWwindow;
struct ImGui_ImplVulkanH_Window;

namespace A3
{
class VulkanRenderBackend;

class Addon_imgui
{
public:
	Addon_imgui( GLFWwindow* window, VulkanRenderBackend* vulkan, int32 screenWidth, int32 screenHeight );

	void renderFrame( GLFWwindow* window, VulkanRenderBackend* vulkan, Scene* scene );

private:
	// @FIXME: Temporary. Remove later.
	void CleanupVulkan( VulkanRenderBackend* vulkan );
	void CleanupVulkanWindow( VulkanRenderBackend* vulkan );
};
}
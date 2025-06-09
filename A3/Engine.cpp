#include "Engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <tuple>
#include <bitset>
#include <span>
#include "Vulkan.h"
#include "PathTracingRenderer.h"
#include "Addon_imgui.h"
#include "Scene.h"

namespace A3
{
static void glfw_error_callback( int error, const char* description )
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

void Engine::Run()
{
    glfwSetErrorCallback( glfw_error_callback );
    glfwInit();

    if( !glfwVulkanSupported() )
    {
        printf( "GLFW: Vulkan Not Supported\n" );
    }

    std::vector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions( &extensions_count );
    for( uint32_t i = 0; i < extensions_count; i++ )
        extensions.push_back( glfw_extensions[ i ] );
    if( ON_DEBUG ) extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );
    GLFWwindow* window = glfwCreateWindow( RenderSettings::screenWidth, RenderSettings::screenHeight, "Vulkan", nullptr, nullptr );

    int32 screenWidth, screenHeight;
    glfwGetFramebufferSize( window, &screenWidth, &screenHeight );

    {
        VulkanRenderBackend gfxBackend( window, extensions, screenWidth, screenHeight );

        Scene scene;
        scene.load("scene.json");

        PathTracingRenderer renderer( &gfxBackend );

        Addon_imgui imgui( window, &gfxBackend, screenWidth, screenHeight );

        while( !glfwWindowShouldClose( window ) )
        {
            glfwPollEvents();

            scene.beginFrame();

            renderer.beginFrame( screenWidth, screenHeight );

            renderer.render( scene );
            imgui.renderFrame( window, &gfxBackend, &scene );

            renderer.endFrame();

            scene.endFrame();
        }
    }

    glfwDestroyWindow( window );
    glfwTerminate();
}
}
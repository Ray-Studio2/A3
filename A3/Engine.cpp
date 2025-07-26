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
#include "CameraObject.h"

#include "MeshObject.h"

namespace A3
{
static void glfw_error_callback( int error, const char* description )
{
    fprintf( stderr, "GLFW Error %d: %s\n", error, description );
}

// TODO: apply delta-time to calibrate camera movement speed
void updateInput(GLFWwindow* window, Scene& scene) {
    static bool mouseRightPressed = false;
    static double prevCursorPosX{ }, prevCursorPosY{ };

    int rightButtonState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

    if (rightButtonState == GLFW_PRESS && !mouseRightPressed) {
        glfwGetCursorPos(window, &prevCursorPosX, &prevCursorPosY);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        mouseRightPressed = true;
    }
    else if (rightButtonState == GLFW_RELEASE && mouseRightPressed) {
        glfwSetCursorPos(window, prevCursorPosX, prevCursorPosY);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouseRightPressed = false;
    }

    if (rightButtonState) {
        CameraObject* camera = scene.getCamera();

        if (glfwGetKey(window, GLFW_KEY_W)) {
            camera->move(GLFW_KEY_W);
            scene.markBufferUpdated();
        }
        if (glfwGetKey(window, GLFW_KEY_A)) {
            camera->move(GLFW_KEY_A);
            scene.markBufferUpdated();
        }
        if (glfwGetKey(window, GLFW_KEY_S)) {
            camera->move(GLFW_KEY_S);
            scene.markBufferUpdated();
        }
        if (glfwGetKey(window, GLFW_KEY_D)) {
            camera->move(GLFW_KEY_D);
            scene.markBufferUpdated();
        }

        static const float camSens = 0.1f;
        static const float RAD = 0.017453f;

        static double x{ }, y{ };
        static float dx{ }, dy{ };

        glfwGetCursorPos(window, &x, &y);

        dx = (static_cast<float>(x) - static_cast<float>(prevCursorPosX)) * camSens * RAD;
        dy = (static_cast<float>(prevCursorPosY) - static_cast<float>(y)) * camSens * RAD;

        if (!floatEqual(dx, 0.0f) || !floatEqual(dy, 0.0f)) {
            camera->rotateView(dx, dy);

            glfwSetCursorPos(window, prevCursorPosX, prevCursorPosY);
            scene.markBufferUpdated();
        }
    }
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

        // Scene must be initialize after VulkanRenderBackend. Cuz using Vulkan Interface function in Scene::load.
        Scene scene;
        scene.load(RenderSettings::sceneFiles[RenderSettings::sceneIdx], gfxBackend); // TODO: Separated ConfigManager & AppSettings class (constants as file paths, resolution, spp, camera info...)
        // TODO: Scene only handles objects, mesh, lightings from Json

        PathTracingRenderer renderer( &gfxBackend );

        Addon_imgui imgui( window, &gfxBackend, screenWidth, screenHeight );

        while( !glfwWindowShouldClose( window ) )
        {
            glfwPollEvents();
            updateInput(window, scene);

            scene.beginFrame();

            renderer.beginFrame( screenWidth, screenHeight );

            renderer.render( scene );
            imgui.renderFrame( window, &gfxBackend, &scene );

            std::vector<MeshObject*> meshObjects = scene.collectMeshObjects();
            for (int i = 0; i < meshObjects.size(); i++) {
                meshObjects[i]->getMaterial()->uploadMaterialParameter(gfxBackend);
            }

            renderer.endFrame();

            scene.endFrame();
        }
    }

    glfwDestroyWindow( window );
    glfwTerminate();
}
}
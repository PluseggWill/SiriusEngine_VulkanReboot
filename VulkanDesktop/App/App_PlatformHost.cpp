#include "App_PlatformHost.h"

#include "../RenderCore/Vk_Renderer.h"
#include "../Util/Util_Logger.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <stdexcept>

App_PlatformHost::~App_PlatformHost() {
    ShutdownWindow();
}

void App_PlatformHost::InitWindow( uint32_t aWidth, uint32_t aHeight, Vk_Renderer& aRenderer ) {
    UtilLogger::Info( "WINDOW", "Initializing GLFW window." );
    glfwInit();
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    myWindow = glfwCreateWindow( static_cast< int >( aWidth ), static_cast< int >( aHeight ), "Vulkan Window", nullptr, nullptr );
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "App_PlatformHost: glfwCreateWindow failed" );
    }
    glfwSetWindowUserPointer( myWindow, &aRenderer );
    glfwSetFramebufferSizeCallback( myWindow, &App_PlatformHost::FramebufferResizeCallback );
    UtilLogger::Info( "WINDOW", "Window created: " + std::to_string( aWidth ) + "x" + std::to_string( aHeight ) );
}

void App_PlatformHost::ShutdownWindow() {
    if ( myWindow != nullptr ) {
        glfwDestroyWindow( myWindow );
        myWindow = nullptr;
    }
    glfwTerminate();
    myHasLastFrameTime = false;
}

bool App_PlatformHost::ShouldClose() const {
    return myWindow != nullptr && glfwWindowShouldClose( myWindow );
}

void App_PlatformHost::RequestClose() {
    if ( myWindow != nullptr ) {
        glfwSetWindowShouldClose( myWindow, GLFW_TRUE );
    }
}

void App_PlatformHost::BeginFrame( Vk_Renderer& aRenderer, float& aOutDeltaSeconds ) {
    glfwPollEvents();
    const auto frameStart = std::chrono::high_resolution_clock::now();
    aOutDeltaSeconds      = 0.0f;
    if ( myHasLastFrameTime ) {
        aOutDeltaSeconds = std::chrono::duration< float >( frameStart - myLastFrameTime ).count();
    }
    myLastFrameTime = frameStart;
    myHasLastFrameTime = true;
    aRenderer.OnPlatformFrameStart( frameStart, aOutDeltaSeconds );
}

void App_PlatformHost::BeginImGuiFrame( Vk_Renderer& aRenderer ) {
    aRenderer.BeginImGuiFrame();
}

void App_PlatformHost::CreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "App_PlatformHost: window not initialized for CreateSurface" );
    }
    if ( glfwCreateWindowSurface( aRhiDevice.myDeviceCtx.myInstance, myWindow, nullptr, &aRhiDevice.myDeviceCtx.mySurface ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "glfwCreateWindowSurface failed." );
        throw std::runtime_error( "failed to create window surface!" );
    }
    UtilLogger::Info( "VULKAN", "Window surface created." );
}

void App_PlatformHost::RecreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( aRhiDevice.myDeviceCtx.mySurface != VK_NULL_HANDLE ) {
        vkDestroySurfaceKHR( aRhiDevice.myDeviceCtx.myInstance, aRhiDevice.myDeviceCtx.mySurface, nullptr );
        aRhiDevice.myDeviceCtx.mySurface = VK_NULL_HANDLE;
    }
    CreateSurface( aRhiDevice );
}

void App_PlatformHost::FramebufferResizeCallback( GLFWwindow* aWindow, int, int ) {
    auto* renderer = reinterpret_cast< Vk_Renderer* >( glfwGetWindowUserPointer( aWindow ) );
    if ( renderer != nullptr ) {
        renderer->NotifyFramebufferResized();
    }
}

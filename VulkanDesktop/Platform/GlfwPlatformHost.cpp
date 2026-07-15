#include "GlfwPlatformHost.h"

#include "../RenderCore/Vk_Renderer.h"
#include "../Util/Util_Logger.h"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>

GlfwPlatformHost::~GlfwPlatformHost() {
    ShutdownWindow();
}

void GlfwPlatformHost::InitWindow( uint32_t aWidth, uint32_t aHeight, Vk_Renderer& aRenderer ) {
    UtilLogger::Info( "WINDOW", "Initializing GLFW window." );
    glfwInit();
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    myWindow = glfwCreateWindow( static_cast< int >( aWidth ), static_cast< int >( aHeight ), "Vulkan Window", nullptr, nullptr );
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "GlfwPlatformHost: glfwCreateWindow failed" );
    }
    glfwSetWindowUserPointer( myWindow, &aRenderer );
    glfwSetFramebufferSizeCallback( myWindow, &GlfwPlatformHost::FramebufferResizeCallback );
    UtilLogger::Info( "WINDOW", "Window created: " + std::to_string( aWidth ) + "x" + std::to_string( aHeight ) );
}

void GlfwPlatformHost::ShutdownWindow() {
    if ( myWindow != nullptr ) {
        glfwDestroyWindow( myWindow );
        myWindow = nullptr;
    }
    glfwTerminate();
    myHasLastFrameTime = false;
}

bool GlfwPlatformHost::ShouldClose() const {
    return myWindow != nullptr && glfwWindowShouldClose( myWindow );
}

void GlfwPlatformHost::RequestClose() {
    if ( myWindow != nullptr ) {
        glfwSetWindowShouldClose( myWindow, GLFW_TRUE );
    }
}

void GlfwPlatformHost::BeginFrame( Vk_Renderer& aRenderer, float& aOutDeltaSeconds ) {
    glfwPollEvents();
    const auto frameStart = std::chrono::high_resolution_clock::now();
    aOutDeltaSeconds      = 0.0f;
    if ( myHasLastFrameTime ) {
        aOutDeltaSeconds = std::chrono::duration< float >( frameStart - myLastFrameTime ).count();
    }
    myLastFrameTime    = frameStart;
    myHasLastFrameTime = true;
    aRenderer.OnPlatformFrameStart( frameStart, aOutDeltaSeconds );
}

void GlfwPlatformHost::BeginImGuiFrame( Vk_Renderer& aRenderer ) {
    aRenderer.BeginImGuiFrame();
}

void GlfwPlatformHost::CreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "GlfwPlatformHost: window not initialized for CreateSurface" );
    }
    if ( glfwCreateWindowSurface( aRhiDevice.myDeviceCtx.myInstance, myWindow, nullptr, &aRhiDevice.myDeviceCtx.mySurface ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "glfwCreateWindowSurface failed." );
        throw std::runtime_error( "failed to create window surface!" );
    }
    UtilLogger::Info( "VULKAN", "Window surface created." );
}

void GlfwPlatformHost::RecreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( aRhiDevice.myDeviceCtx.mySurface != VK_NULL_HANDLE ) {
        vkDestroySurfaceKHR( aRhiDevice.myDeviceCtx.myInstance, aRhiDevice.myDeviceCtx.mySurface, nullptr );
        aRhiDevice.myDeviceCtx.mySurface = VK_NULL_HANDLE;
    }
    CreateSurface( aRhiDevice );
}

void GlfwPlatformHost::FramebufferResizeCallback( GLFWwindow* aWindow, int, int ) {
    auto* renderer = reinterpret_cast< Vk_Renderer* >( glfwGetWindowUserPointer( aWindow ) );
    if ( renderer != nullptr ) {
        renderer->NotifyFramebufferResized();
    }
}

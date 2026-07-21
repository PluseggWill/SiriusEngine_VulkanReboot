#include "Pf_GlfwPlatformHost.h"

#include "../RenderCore/Vk_RhiDevice.h"
#include "../Util/Util_Logger.h"

#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>

Pf_GlfwPlatformHost::~Pf_GlfwPlatformHost() {
    ShutdownWindow();
}

void Pf_GlfwPlatformHost::InitWindow( uint32_t aWidth, uint32_t aHeight, const Pf_FrameHooks& aFrameHooks ) {
    UtilLogger::Info( "WINDOW", "Initializing GLFW window." );
    glfwInit();
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    myWindow = glfwCreateWindow( static_cast< int >( aWidth ), static_cast< int >( aHeight ), "Vulkan Window", nullptr, nullptr );
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "Pf_GlfwPlatformHost: glfwCreateWindow failed" );
    }
    myFrameHooks = aFrameHooks;
    glfwSetWindowUserPointer( myWindow, this );
    glfwSetFramebufferSizeCallback( myWindow, &Pf_GlfwPlatformHost::FramebufferResizeCallback );
    UtilLogger::Info( "WINDOW", "Window created: " + std::to_string( aWidth ) + "x" + std::to_string( aHeight ) );
}

void Pf_GlfwPlatformHost::ShutdownWindow() {
    if ( myWindow != nullptr ) {
        glfwDestroyWindow( myWindow );
        myWindow = nullptr;
    }
    glfwTerminate();
    myHasLastFrameTime = false;
    myFrameHooks       = {};
}

bool Pf_GlfwPlatformHost::ShouldClose() const {
    return myWindow != nullptr && glfwWindowShouldClose( myWindow );
}

void Pf_GlfwPlatformHost::RequestClose() {
    if ( myWindow != nullptr ) {
        glfwSetWindowShouldClose( myWindow, GLFW_TRUE );
    }
}

void Pf_GlfwPlatformHost::BeginFrame( float& aOutDeltaSeconds ) {
    glfwPollEvents();
    const auto frameStart = std::chrono::high_resolution_clock::now();
    aOutDeltaSeconds      = 0.0f;
    if ( myHasLastFrameTime ) {
        aOutDeltaSeconds = std::chrono::duration< float >( frameStart - myLastFrameTime ).count();
    }
    myLastFrameTime    = frameStart;
    myHasLastFrameTime = true;
    if ( myFrameHooks.myOnFrameStart != nullptr ) {
        myFrameHooks.myOnFrameStart( myFrameHooks.myUser, frameStart, aOutDeltaSeconds );
    }
}

void Pf_GlfwPlatformHost::BeginImGuiFrame() {
    if ( myFrameHooks.myOnImGuiNewFrame != nullptr ) {
        myFrameHooks.myOnImGuiNewFrame( myFrameHooks.myUser );
    }
}

void Pf_GlfwPlatformHost::CreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( myWindow == nullptr ) {
        throw std::runtime_error( "Pf_GlfwPlatformHost: window not initialized for CreateSurface" );
    }
    if ( glfwCreateWindowSurface( aRhiDevice.myDeviceCtx.myInstance, myWindow, nullptr, &aRhiDevice.myDeviceCtx.mySurface ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "glfwCreateWindowSurface failed." );
        throw std::runtime_error( "failed to create window surface!" );
    }
    UtilLogger::Info( "VULKAN", "Window surface created." );
}

void Pf_GlfwPlatformHost::RecreateSurface( Vk_RhiDevice& aRhiDevice ) {
    if ( aRhiDevice.myDeviceCtx.mySurface != VK_NULL_HANDLE ) {
        vkDestroySurfaceKHR( aRhiDevice.myDeviceCtx.myInstance, aRhiDevice.myDeviceCtx.mySurface, nullptr );
        aRhiDevice.myDeviceCtx.mySurface = VK_NULL_HANDLE;
    }
    CreateSurface( aRhiDevice );
}

void Pf_GlfwPlatformHost::FramebufferResizeCallback( GLFWwindow* aWindow, int, int ) {
    auto* host = reinterpret_cast< Pf_GlfwPlatformHost* >( glfwGetWindowUserPointer( aWindow ) );
    if ( host != nullptr && host->myFrameHooks.myOnFramebufferResized != nullptr ) {
        host->myFrameHooks.myOnFramebufferResized( host->myFrameHooks.myUser );
    }
}

#include "Vk_PlatformFrame.h"

#include "../Util/Util_Logger.h"
#include "Vk_Core.h"

void Vk_PlatformFrame::InitWindow( Vk_Core& aCore ) {
    UtilLogger::Info( "WINDOW", "Initializing GLFW window." );
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    aCore.myPlatformCtx.myWindow = glfwCreateWindow( aCore.myPlatformCtx.myWidth, aCore.myPlatformCtx.myHeight, "Vulkan Window", nullptr, nullptr );
    UtilLogger::Info( "WINDOW", "Window created: " + std::to_string( aCore.myPlatformCtx.myWidth ) + "x" + std::to_string( aCore.myPlatformCtx.myHeight ) );
    glfwSetWindowUserPointer( aCore.myPlatformCtx.myWindow, &aCore );
    glfwSetFramebufferSizeCallback( aCore.myPlatformCtx.myWindow, Vk_Core::FramebufferResizeCallback );
}

void Vk_PlatformFrame::BeginFrame( Vk_Core& aCore, float& aOutDeltaSeconds ) {
    glfwPollEvents();

    const auto frameStart = std::chrono::high_resolution_clock::now();
    aOutDeltaSeconds      = 0.0f;
    if ( aCore.myPlatformCtx.myHasLastFrameTime ) {
        aOutDeltaSeconds = std::chrono::duration< float >( frameStart - aCore.myPlatformCtx.myLastFrameTime ).count();
        aCore.myFrameStats.PushFrameTime( aOutDeltaSeconds * 1000.f );
    }
    aCore.myPlatformCtx.myLastFrameTime    = frameStart;
    aCore.myPlatformCtx.myHasLastFrameTime = true;
    aCore.myPlatformCtx.myImGuiLayer.NewFrame();
}

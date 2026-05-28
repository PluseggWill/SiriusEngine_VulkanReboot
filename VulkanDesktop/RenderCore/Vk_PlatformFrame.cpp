#include "Vk_PlatformFrame.h"

#include "Vk_Core.h"
#include "../Util/Util_Logger.h"

void Vk_PlatformFrame::InitWindow( Vk_Core& aCore ) {
    UtilLogger::Info( "WINDOW", "Initializing GLFW window." );
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    aCore.myWindow = glfwCreateWindow( aCore.myWidth, aCore.myHeight, "Vulkan Window", nullptr, nullptr );
    UtilLogger::Info( "WINDOW", "Window created: " + std::to_string( aCore.myWidth ) + "x" + std::to_string( aCore.myHeight ) );
    glfwSetWindowUserPointer( aCore.myWindow, &aCore );
    glfwSetFramebufferSizeCallback( aCore.myWindow, Vk_Core::FramebufferResizeCallback );
}

void Vk_PlatformFrame::BeginFrame( Vk_Core& aCore, float& aOutDeltaSeconds ) {
    glfwPollEvents();

    const auto frameStart = std::chrono::high_resolution_clock::now();
    aOutDeltaSeconds      = 0.0f;
    if ( aCore.myHasLastFrameTime ) {
        aOutDeltaSeconds = std::chrono::duration< float >( frameStart - aCore.myLastFrameTime ).count();
        aCore.myFrameStats.PushFrameTime( aOutDeltaSeconds * 1000.f );
    }
    aCore.myLastFrameTime    = frameStart;
    aCore.myHasLastFrameTime = true;
    aCore.myImGuiLayer.NewFrame();
}

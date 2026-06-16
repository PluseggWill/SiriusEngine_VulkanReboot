#pragma once

#include "../RenderCore/Vk_RhiDevice.h"

#include <chrono>

struct GLFWwindow;
class Vk_Renderer;

class App_PlatformHost {
public:
    ~App_PlatformHost();

    void InitWindow( uint32_t aWidth, uint32_t aHeight, Vk_Renderer& aRenderer );
    void ShutdownWindow();

    bool ShouldClose() const;
    void RequestClose();

    void BeginFrame( Vk_Renderer& aRenderer, float& aOutDeltaSeconds );
    void BeginImGuiFrame( Vk_Renderer& aRenderer );

    void CreateSurface( Vk_RhiDevice& aRhiDevice );
    void RecreateSurface( Vk_RhiDevice& aRhiDevice );

    GLFWwindow* GetWindow() const { return myWindow; }

private:
    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

    GLFWwindow* myWindow = nullptr;
    std::chrono::high_resolution_clock::time_point myLastFrameTime{};
    bool myHasLastFrameTime = false;
};

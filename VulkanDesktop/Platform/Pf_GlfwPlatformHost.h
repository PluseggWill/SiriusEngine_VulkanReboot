#pragma once

#include "Pf_PlatformHost.h"

#include <chrono>

// GLFW-backed platform host implementation used by VulkanDesktop runtime.
class Pf_GlfwPlatformHost final : public Pf_PlatformHost {
public:
    ~Pf_GlfwPlatformHost() override;

    void InitWindow( uint32_t aWidth, uint32_t aHeight, Vk_Renderer& aRenderer ) override;
    void ShutdownWindow() override;

    bool ShouldClose() const override;
    void RequestClose() override;

    void BeginFrame( Vk_Renderer& aRenderer, float& aOutDeltaSeconds ) override;
    void BeginImGuiFrame( Vk_Renderer& aRenderer ) override;

    void CreateSurface( Vk_RhiDevice& aRhiDevice ) override;
    void RecreateSurface( Vk_RhiDevice& aRhiDevice ) override;

    GLFWwindow* GetWindow() const override {
        return myWindow;
    }

private:
    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

    GLFWwindow*                                    myWindow = nullptr;
    std::chrono::high_resolution_clock::time_point myLastFrameTime{};
    bool                                           myHasLastFrameTime = false;
};

#pragma once

#include <cstdint>

struct GLFWwindow;
class Vk_Renderer;
class Vk_RhiDevice;

// Platform abstraction between App orchestration and RenderCore window/surface lifecycle.
// GLFWwindow remains exposed for the current input/UI path; RenderCore depends only on this seam.
class PlatformHost {
public:
    virtual ~PlatformHost() = default;

    virtual void InitWindow( uint32_t aWidth, uint32_t aHeight, Vk_Renderer& aRenderer ) = 0;
    virtual void ShutdownWindow()                                                         = 0;

    virtual bool ShouldClose() const = 0;
    virtual void RequestClose()      = 0;

    virtual void BeginFrame( Vk_Renderer& aRenderer, float& aOutDeltaSeconds ) = 0;
    virtual void BeginImGuiFrame( Vk_Renderer& aRenderer )                      = 0;

    virtual void CreateSurface( Vk_RhiDevice& aRhiDevice )   = 0;
    virtual void RecreateSurface( Vk_RhiDevice& aRhiDevice ) = 0;

    virtual GLFWwindow* GetWindow() const = 0;
};

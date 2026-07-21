#pragma once

#include "Pf_FrameHooks.h"

#include <cstdint>

struct GLFWwindow;
class Vk_RhiDevice;

// Platform abstraction between App orchestration and RenderCore window/surface lifecycle.
// Frame callbacks use Pf_FrameHooks; RenderCore depends only on this seam (no Vk_Renderer&).
class Pf_PlatformHost {
public:
    virtual ~Pf_PlatformHost() = default;

    virtual void InitWindow( uint32_t aWidth, uint32_t aHeight, const Pf_FrameHooks& aFrameHooks ) = 0;
    virtual void ShutdownWindow()                                                                  = 0;

    virtual bool ShouldClose() const = 0;
    virtual void RequestClose()      = 0;

    virtual void BeginFrame( float& aOutDeltaSeconds ) = 0;
    virtual void BeginImGuiFrame()                     = 0;

    virtual void CreateSurface( Vk_RhiDevice& aRhiDevice )   = 0;
    virtual void RecreateSurface( Vk_RhiDevice& aRhiDevice ) = 0;

    virtual GLFWwindow* GetWindow() const = 0;
};

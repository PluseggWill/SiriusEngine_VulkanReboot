#pragma once

#include <cstdint>

#include "Vk_FrameResult.h"

class Vk_Core;
struct Vk_FrameData;

// Vk_SwapchainHost: owns swapchain-host orchestration slice (phase-2 #3).
// Scope: swapchain/render-pass/depth-color/framebuffers and recreate flow.
class Vk_SwapchainHost {
public:
    static void Init( Vk_Core& aCore );
    static void Recreate( Vk_Core& aCore );
    static bool AcquireNextImage( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex );
    static Vk_FrameResult SubmitAndPresent( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t anImageIndex );
    static void CreateSwapChain( Vk_Core& aCore );
    static void CreateRenderPass( Vk_Core& aCore );
    static void CreateFrameBuffers( Vk_Core& aCore );
    static void CreateDepthResources( Vk_Core& aCore );
    static void CreateColorResources( Vk_Core& aCore );
};

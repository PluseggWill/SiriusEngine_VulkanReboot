#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Vk_FrameResult.h"

class Vk_Core;
struct Vk_FrameData;

// Vk_SwapchainHost: owns swapchain-host orchestration slice (phase-2 #3).
// Scope: swapchain/render-pass/depth-color/framebuffers and recreate flow.
// Acquire: Khronos-style OUT_OF_DATE/SUBOPTIMAL → Recreate + one retry; fence reset stays in DrawFrameGpu.
// INVARIANT: MAX_FRAMES_IN_FLIGHT (Vk_Core.h) <= swapchain image count chosen in CreateSwapChain.
class Vk_SwapchainHost {
public:
    static void Init( Vk_Core& aCore );
    static void Recreate( Vk_Core& aCore );
    // Extent precheck before heavy recreate (B1); returns true when extent changed or resize flag set.
    static bool NeedsSwapchainRebuild( Vk_Core& aCore, VkExtent2D& aOutTargetExtent );
    static bool AcquireNextImage( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex );
    static Vk_FrameResult SubmitAndPresent( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t anImageIndex );
    // aSupersededSwapChain: WSI handle kept for createInfo.oldSwapchain during recreate (destroyed after new chain exists).
    static void CreateSwapChain( Vk_Core& aCore, VkSwapchainKHR aSupersededSwapChain = VK_NULL_HANDLE );
    static void CreateRenderPass( Vk_Core& aCore );
    static void CreateFrameBuffers( Vk_Core& aCore );
    static void CreateDepthResources( Vk_Core& aCore );
    static void CreateColorResources( Vk_Core& aCore );
};

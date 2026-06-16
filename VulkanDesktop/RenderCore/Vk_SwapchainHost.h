#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Vk_FrameResult.h"

class Vk_Renderer;
struct Vk_FrameData;

// Vk_SwapchainHost: owns swapchain-host orchestration slice (phase-2 #3).
// Scope: swapchain/render-pass/depth-color/framebuffers and recreate flow.
// Acquire: Khronos-style OUT_OF_DATE/SUBOPTIMAL → Recreate + one retry; fence reset stays in DrawFrameGpu.
// INVARIANT: MAX_FRAMES_IN_FLIGHT (Vk_FrameLimits.h) <= swapchain image count chosen in CreateSwapChain.
class Vk_SwapchainHost {
public:
    static void Init( Vk_Renderer& aCore );
    static void Recreate( Vk_Renderer& aCore );
    // B2 three-layer split — orchestrator calls these in order; log rebuild layer=wsi|extent|pipeline.
    static void RecreateWsiOnly( Vk_Renderer& aCore, VkSwapchainKHR aSupersededSwapChain = VK_NULL_HANDLE );
    static void RebuildExtentDependentResources( Vk_Renderer& aCore, bool aIncludeRenderPass );
    static void RebuildScenePipelinesIfNeeded( Vk_Renderer& aCore );
    static bool HandleSurfaceLost( Vk_Renderer& aCore );
    // Extent precheck before heavy recreate (B1); returns true when extent changed or resize flag set.
    static bool           NeedsSwapchainRebuild( Vk_Renderer& aCore, VkExtent2D& aOutTargetExtent );
    static bool           AcquireNextImage( Vk_Renderer& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex );
    static Vk_FrameResult SubmitAndPresent( Vk_Renderer& aCore, const Vk_FrameData& aFrameData, uint32_t anImageIndex, float* aOutPresentWaitMs = nullptr );
    // aSupersededSwapChain: WSI handle kept for createInfo.oldSwapchain during recreate (destroyed after new chain exists).
    static void CreateSwapChain( Vk_Renderer& aCore, VkSwapchainKHR aSupersededSwapChain = VK_NULL_HANDLE );
    static void CreateRenderPass( Vk_Renderer& aCore );
    static void CreateFrameBuffers( Vk_Renderer& aCore );
    static void CreateDepthResources( Vk_Renderer& aCore );
    static void CreateColorResources( Vk_Renderer& aCore );
};

#pragma once

class Vk_Core;

// Vk_SwapchainHost: owns swapchain-host orchestration slice (phase-2 #3).
// Scope: swapchain/render-pass/depth-color/framebuffers and recreate flow.
class Vk_SwapchainHost {
public:
    static void Init( Vk_Core& aCore );
    static void Recreate( Vk_Core& aCore );
};

#pragma once

#include "Vk_Types.h"

#include "Vk_Types.h"
#include <vector>
#include <vulkan/vulkan.h>

// Swapchain / render pass / MSAA targets — consumed by Vk_SwapchainHost and scene record.
struct Vk_SwapchainContext {
    VkSampleCountFlagBits myMSAASamples = VK_SAMPLE_COUNT_1_BIT;

    VkSwapchainKHR mySwapChain = VK_NULL_HANDLE;
    VkFormat       mySwapChainImageFormat{};
    VkExtent2D     mySwapChainExtent{};

    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;

    VkRenderPass myRenderPass              = VK_NULL_HANDLE;
    VkRenderPass myHybridResolveRenderPass = VK_NULL_HANDLE;  // depth LOAD after G-buffer depth copy (HybridDeferred transparent)

    Gfx_Texture myDepthTexture;
    Gfx_Texture myColorTexture;

    Vk_DeletionQueue mySwapChainDeletionQueue;

    bool myFramebufferResized = false;
    // After first Recreate(): swapchain deletor moves to index 1 when render pass is reused (see DiscardPendingSwapchainDeletor).
    bool myHasRecreateOnce = false;
};

#include "Vk_SwapchainHost.h"

#include "../Util/Util_Logger.h"
#include "Vk_ClusterBuildPass.h"
#include "Vk_Core.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_GBufferPass.h"
#include "Vk_GfxPipelineCache.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kPreferredSwapchainImageCount = 3;

VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha( const VkSurfaceCapabilitiesKHR& aCapabilities ) {
    static constexpr VkCompositeAlphaFlagBitsKHR kOrder[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    };
    for ( const VkCompositeAlphaFlagBitsKHR flag : kOrder ) {
        if ( aCapabilities.supportedCompositeAlpha & flag ) {
            return flag;
        }
    }
    throw std::runtime_error( "no supported composite alpha mode for surface" );
}

const char* CompositeAlphaName( VkCompositeAlphaFlagBitsKHR aMode ) {
    switch ( aMode ) {
    case VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:
        return "OPAQUE";
    case VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR:
        return "INHERIT";
    case VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR:
        return "PRE_MULTIPLIED";
    case VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR:
        return "POST_MULTIPLIED";
    default:
        return "UNKNOWN";
    }
}

uint32_t ChooseSwapchainImageCount( const VkSurfaceCapabilitiesKHR& aCapabilities ) {
    uint32_t imageCount = std::max( aCapabilities.minImageCount, kPreferredSwapchainImageCount );
    if ( aCapabilities.maxImageCount > 0 && imageCount > aCapabilities.maxImageCount ) {
        imageCount = aCapabilities.maxImageCount;
    }
    return imageCount;
}

VkResult TryAcquireOnce( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex ) {
    return vkAcquireNextImageKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, UINT64_MAX, aFrameData.myPresentSemaphore, VK_NULL_HANDLE, &anOutImageIndex );
}

void DestroySwapchainImageViews( Vk_Core& aCore ) {
    const VkDevice device = aCore.myDeviceCtx.myDevice;
    for ( const VkImageView imageView : aCore.mySwapchainCtx.mySwapChainImageViews ) {
        vkDestroyImageView( device, imageView, nullptr );
    }
    aCore.mySwapchainCtx.mySwapChainImageViews.clear();
    aCore.mySwapchainCtx.mySwapChainImages.clear();
}

bool RenderPassNeedsRebuild( Vk_Core& aCore ) {
    const Vk_SwapChainSupportDetails support = aCore.QuerySwapChainSupport( aCore.myDeviceCtx.myPhysicalDevice );
    const VkSurfaceFormatKHR         format  = aCore.ChooseSwapSurfaceFormat( support.myFormats );
    return format.format != aCore.mySwapchainCtx.mySwapChainImageFormat;
}

// Maps queue results for per-frame submit/present/acquire; OUT_OF_DATE is handled by callers via Recreate.
Vk_FrameResult ClassifyQueueResult( VkResult aResult, const char* aOperation ) {
    if ( aResult == VK_SUCCESS ) {
        return Vk_FrameResult::Ok;
    }
    if ( aResult == VK_ERROR_DEVICE_LOST ) {
        UtilLogger::Error( "VK", std::string( aOperation ) + " returned VK_ERROR_DEVICE_LOST." );
        return Vk_FrameResult::RequestShutdown;
    }
    UtilLogger::Error( "VK", std::string( aOperation ) + " failed (VkResult=" + std::to_string( static_cast< int >( aResult ) ) + ")." );
    return Vk_FrameResult::SkipFrame;
}

}  // namespace

bool Vk_SwapchainHost::HandleSurfaceLost( Vk_Core& aCore ) {
    UtilLogger::Warn( "SWAPCHAIN", "VK_ERROR_SURFACE_LOST_KHR; recreating surface and swapchain." );
    vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    aCore.RecreateSurface();
    Recreate( aCore );
    return true;
}

void Vk_SwapchainHost::Init( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Vk_SwapchainHost::Init." );
    CreateSwapChain( aCore );
    CreateRenderPass( aCore );
    CreateColorResources( aCore );
    CreateDepthResources( aCore );
    CreateFrameBuffers( aCore );
}

bool Vk_SwapchainHost::NeedsSwapchainRebuild( Vk_Core& aCore, VkExtent2D& aOutTargetExtent ) {
    const Vk_SwapChainSupportDetails support = aCore.QuerySwapChainSupport( aCore.myDeviceCtx.myPhysicalDevice );
    aOutTargetExtent                         = aCore.ChooseSwapExtent( support.myCapabilities );
    const bool extentChanged =
        aOutTargetExtent.width != aCore.mySwapchainCtx.mySwapChainExtent.width || aOutTargetExtent.height != aCore.mySwapchainCtx.mySwapChainExtent.height;
    return extentChanged || aCore.mySwapchainCtx.myFramebufferResized;
}

// Drop the pending swapchain deletor without running it (superseded chain uses createInfo.oldSwapchain).
// Init order: [swapchain, renderpass, depth, (+msaa), framebuffers]. First recreate: pop_front (swapchain at 0).
// After renderPass=reuse cycle: [renderpass, swapchain, depth, framebuffers] — swapchain at index 1.
void DiscardPendingSwapchainDeletor( Vk_Core& aCore ) {
    auto& deletors = aCore.mySwapchainCtx.mySwapChainDeletionQueue.myDeletors;
    if ( deletors.empty() ) {
        return;
    }
    if ( !aCore.mySwapchainCtx.myHasRecreateOnce ) {
        deletors.pop_front();
        return;
    }
    if ( deletors.size() >= 2 ) {
        deletors.erase( deletors.begin() + 1 );
        return;
    }
    deletors.pop_back();
}

void Vk_SwapchainHost::RecreateWsiOnly( Vk_Core& aCore, VkSwapchainKHR aSupersededSwapChain ) {
    UtilLogger::Info( "SWAPCHAIN", "rebuild layer=wsi" );
    aCore.myPlatformCtx.myImGuiLayer.DestroySwapchainResources();

    DestroySwapchainImageViews( aCore );
    DiscardPendingSwapchainDeletor( aCore );
    CreateSwapChain( aCore, aSupersededSwapChain );

    const uint32_t imageCount    = static_cast< uint32_t >( aCore.mySwapchainCtx.mySwapChainImageViews.size() );
    const uint32_t minImageCount = std::max( 2u, imageCount );
    aCore.myPlatformCtx.myImGuiLayer.CreateSwapchainResources( aCore.mySwapchainCtx.mySwapChainImageFormat, aCore.mySwapchainCtx.mySwapChainExtent,
                                                               aCore.mySwapchainCtx.mySwapChainImageViews, imageCount, minImageCount );
}

void RunExtentDeletorsBeforeSwapchain( Vk_Core& aCore, bool aIncludeRenderPass ) {
    Vk_DeletionQueue& queue = aCore.mySwapchainCtx.mySwapChainDeletionQueue;

    if ( aIncludeRenderPass && !queue.myDeletors.empty() ) {
        std::function< void() > fn = std::move( queue.myDeletors.front() );
        queue.myDeletors.pop_front();
        fn();
    }

    size_t attachmentDeletorCount = 2;  // depth + framebuffers
    if ( aCore.mySwapchainCtx.myMSAASamples != VK_SAMPLE_COUNT_1_BIT ) {
        ++attachmentDeletorCount;  // MSAA color
    }

    for ( size_t i = 0; i < attachmentDeletorCount; ++i ) {
        queue.popSecondFromBackAndRun();  // keep swapchain deletor at back (just pushed in RecreateWsiOnly)
    }
}

void Vk_SwapchainHost::RebuildExtentDependentResources( Vk_Core& aCore, bool aIncludeRenderPass ) {
    UtilLogger::Info( "SWAPCHAIN", "rebuild layer=extent renderPass=" + std::string( aIncludeRenderPass ? "yes" : "reuse" ) );
    RunExtentDeletorsBeforeSwapchain( aCore, aIncludeRenderPass );

    if ( aIncludeRenderPass ) {
        CreateRenderPass( aCore );
    }
    CreateColorResources( aCore );
    CreateDepthResources( aCore );
    CreateFrameBuffers( aCore );
    Vk_GBufferPass::RecreateForExtent( aCore );
    Vk_ClusterBuildPass::RecreateForExtent( aCore );
    Vk_DeferredLightingPass::RecreateForExtent( aCore );
}

void Vk_SwapchainHost::RebuildScenePipelinesIfNeeded( Vk_Core& aCore ) {
    if ( !aCore.HasLoadedScene() ) {
        return;
    }
    UtilLogger::Info( "SWAPCHAIN", "rebuild layer=pipeline" );
    Vk_GfxPipelineCache::DestroyScenePipelines( aCore );
    Vk_GfxPipelineCache::InitScenePipelines( aCore );
    aCore.RefreshMaterialPipelinesAfterSwapchainRecreate();
}

void Vk_SwapchainHost::Recreate( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Recreating swapchain." );
    int width = 0, height = 0;
    glfwGetFramebufferSize( aCore.myPlatformCtx.myWindow, &width, &height );
    while ( width == 0 || height == 0 ) {
        glfwGetFramebufferSize( aCore.myPlatformCtx.myWindow, &width, &height );
        glfwWaitEvents();
    }

    const VkExtent2D previousExtent = aCore.mySwapchainCtx.mySwapChainExtent;
    VkExtent2D       targetExtent{};
    const bool       needsRebuild = NeedsSwapchainRebuild( aCore, targetExtent );
    if ( aCore.mySwapchainCtx.myFramebufferResized ) {
        UtilLogger::Info( "SWAPCHAIN", "Recreate precheck: framebuffer resize flag set." );
    }
    else if ( !needsRebuild ) {
        UtilLogger::Info( "SWAPCHAIN", "Recreate precheck: extent unchanged (suboptimal/out-of-date path)." );
    }
    else {
        UtilLogger::Info( "SWAPCHAIN", "Recreate precheck: extent " + std::to_string( previousExtent.width ) + "x" + std::to_string( previousExtent.height ) + " -> "
                                           + std::to_string( targetExtent.width ) + "x" + std::to_string( targetExtent.height ) );
    }

    const bool extentChanged                  = targetExtent.width != previousExtent.width || targetExtent.height != previousExtent.height;
    aCore.mySwapchainCtx.myFramebufferResized = false;

    const VkSwapchainKHR supersededSwapChain = aCore.mySwapchainCtx.mySwapChain;

    vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );

    const bool includeRenderPass = RenderPassNeedsRebuild( aCore );
    RecreateWsiOnly( aCore, supersededSwapChain );
    RebuildExtentDependentResources( aCore, includeRenderPass );

    // Scene lit pipelines (and myPipelineLayout) are recreated on extent/format change. G-buffer MRT pipelines
    // embed that layout — refresh them after scene rebuild so opaque writes albedo + normal (not forward-lit RT0 only).
    if ( extentChanged || includeRenderPass ) {
        RebuildScenePipelinesIfNeeded( aCore );
        if ( Vk_GBufferPass::IsActive( aCore ) ) {
            Vk_GBufferPass::RecreatePipelines( aCore );
        }
    }

    aCore.myCamera.SetAspect( static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.width ) / static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.height ) );
    aCore.mySwapchainCtx.myHasRecreateOnce = true;
    UtilLogger::Info( "SWAPCHAIN", "Swapchain recreation completed." );
}

bool Vk_SwapchainHost::AcquireNextImage( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex ) {
    static bool sAcquirePathLogged = false;

    // CONTRACT: caller already vkWaitForFences; do not vkResetFences here — early return leaves fence signaled.
    VkResult result = TryAcquireOnce( aCore, aFrameData, anOutImageIndex );
    if ( result == VK_ERROR_SURFACE_LOST_KHR ) {
        HandleSurfaceLost( aCore );
        return false;
    }
    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
        UtilLogger::Warn( "SWAPCHAIN", "Acquire outdated/suboptimal; recreating and retrying acquire." );
        Recreate( aCore );
        result = TryAcquireOnce( aCore, aFrameData, anOutImageIndex );
    }

    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        return false;
    }
    if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
        ( void )ClassifyQueueResult( result, "vkAcquireNextImageKHR" );
        return false;
    }

    if ( !sAcquirePathLogged ) {
        UtilLogger::Info( "SWAPCHAIN", "Acquire path delegated to Vk_SwapchainHost." );
        sAcquirePathLogged = true;
    }
    return true;
}

Vk_FrameResult Vk_SwapchainHost::SubmitAndPresent( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t anImageIndex, float* aOutPresentWaitMs ) {
    static bool sPresentPathLogged = false;
    if ( aOutPresentWaitMs != nullptr ) {
        *aOutPresentWaitMs = 0.f;
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphore[] = { aFrameData.myPresentSemaphore };
    VkPipelineStageFlags waitStages[]    = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount        = 1;
    submitInfo.pWaitSemaphores           = waitSemaphore;
    submitInfo.pWaitDstStageMask         = waitStages;
    submitInfo.commandBufferCount        = 1;
    submitInfo.pCommandBuffers           = &aFrameData.myCommandBuffer;

    VkSemaphore signalSemaphores[]  = { aFrameData.myRenderSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    const VkResult submitResult = vkQueueSubmit( aCore.myDeviceCtx.myGraphicsQueue, 1, &submitInfo, aFrameData.myRenderFence );
    if ( submitResult != VK_SUCCESS ) {
        return ClassifyQueueResult( submitResult, "vkQueueSubmit" );
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = { aCore.mySwapchainCtx.mySwapChain };
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;
    presentInfo.pImageIndices   = &anImageIndex;
    presentInfo.pResults        = nullptr;

    // Wall-clock block inside vkQueuePresentKHR (FIFO vsync wait; optional Util_FrameStats breakdown).
    const auto     presentStart = std::chrono::high_resolution_clock::now();
    const VkResult result       = vkQueuePresentKHR( aCore.myDeviceCtx.myPresentQueue, &presentInfo );
    if ( aOutPresentWaitMs != nullptr ) {
        *aOutPresentWaitMs = std::chrono::duration< float, std::milli >( std::chrono::high_resolution_clock::now() - presentStart ).count();
    }
    if ( result == VK_ERROR_SURFACE_LOST_KHR ) {
        HandleSurfaceLost( aCore );
        return Vk_FrameResult::SkipFrame;
    }
    // Resize flag checked after present (tutorial): keeps image-acquired semaphore pairing consistent.
    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || aCore.mySwapchainCtx.myFramebufferResized ) {
        aCore.mySwapchainCtx.myFramebufferResized = false;
        UtilLogger::Warn( "SWAPCHAIN", "Present reported outdated/suboptimal framebuffer. Recreating swapchain." );
        Recreate( aCore );
        return Vk_FrameResult::SkipFrame;
    }
    if ( result != VK_SUCCESS ) {
        return ClassifyQueueResult( result, "vkQueuePresentKHR" );
    }
    if ( !sPresentPathLogged ) {
        UtilLogger::Info( "SWAPCHAIN", "Present path delegated to Vk_SwapchainHost." );
        sPresentPathLogged = true;
    }
    return Vk_FrameResult::Ok;
}

void Vk_SwapchainHost::CreateSwapChain( Vk_Core& aCore, VkSwapchainKHR aSupersededSwapChain ) {
    UtilLogger::Info( "SWAPCHAIN", "Creating swapchain." );
    const Vk_SwapChainSupportDetails  swapChainSupport = aCore.QuerySwapChainSupport( aCore.myDeviceCtx.myPhysicalDevice );
    const VkSurfaceFormatKHR          surfaceFormat    = aCore.ChooseSwapSurfaceFormat( swapChainSupport.myFormats );
    const VkPresentModeKHR            presentMode      = aCore.ChooseSwapPresentMode( swapChainSupport.myPresentModes );
    const VkExtent2D                  extent           = aCore.ChooseSwapExtent( swapChainSupport.myCapabilities );
    uint32_t                          imageCount       = ChooseSwapchainImageCount( swapChainSupport.myCapabilities );
    const VkCompositeAlphaFlagBitsKHR compositeAlpha   = ChooseCompositeAlpha( swapChainSupport.myCapabilities );
    if ( imageCount < static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT ) ) {
        throw std::runtime_error( "swapchain imageCount < MAX_FRAMES_IN_FLIGHT" );
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = aCore.myDeviceCtx.mySurface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // Graphics and present queues can differ on some GPUs.
    const uint32_t queueFamilyIndices[] = { aCore.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), aCore.myDeviceCtx.myQueueFamilyIndices.myPresentFamily.value() };
    if ( aCore.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily != aCore.myDeviceCtx.myQueueFamilyIndices.myPresentFamily ) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform   = swapChainSupport.myCapabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    // Hand off superseded chain to WSI; still safe with vkDeviceWaitIdle in Recreate (baseline stall).
    createInfo.oldSwapchain = aSupersededSwapChain;
    if ( vkCreateSwapchainKHR( aCore.myDeviceCtx.myDevice, &createInfo, nullptr, &aCore.mySwapchainCtx.mySwapChain ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create swap chain!" );
    }
    if ( aSupersededSwapChain != VK_NULL_HANDLE ) {
        vkDestroySwapchainKHR( aCore.myDeviceCtx.myDevice, aSupersededSwapChain, nullptr );
    }
    const uint32_t requestedImageCount = imageCount;
    vkGetSwapchainImagesKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, &imageCount, nullptr );
    aCore.mySwapchainCtx.mySwapChainImages.resize( imageCount );
    vkGetSwapchainImagesKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, &imageCount, aCore.mySwapchainCtx.mySwapChainImages.data() );
    aCore.mySwapchainCtx.mySwapChainImageFormat = surfaceFormat.format;
    aCore.mySwapchainCtx.mySwapChainExtent      = extent;
    UtilLogger::Info( "SWAPCHAIN", "imageCount=" + std::to_string( requestedImageCount ) + " compositeAlpha=" + CompositeAlphaName( compositeAlpha )
                                       + " extent=" + std::to_string( extent.width ) + "x" + std::to_string( extent.height ) );
    aCore.mySwapchainCtx.mySwapChainImageViews.resize( imageCount );
    for ( size_t i = 0; i < imageCount; ++i ) {
        aCore.mySwapchainCtx.mySwapChainImageViews[ i ] =
            aCore.CreateImageView( aCore.mySwapchainCtx.mySwapChainImages[ i ], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT );
    }
    const VkDevice       device     = aCore.myDeviceCtx.myDevice;
    const VkSwapchainKHR swapchain  = aCore.mySwapchainCtx.mySwapChain;
    const auto           imageViews = aCore.mySwapchainCtx.mySwapChainImageViews;
    // Registered at end of WSI step; extent rebuild runs attachment deletors before it via popSecondFromBackAndRun.
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, swapchain, imageViews ]() {
        for ( const VkImageView imageView : imageViews ) {
            vkDestroyImageView( device, imageView, nullptr );
        }
        vkDestroySwapchainKHR( device, swapchain, nullptr );
    } );
}

void Vk_SwapchainHost::CreateRenderPass( Vk_Core& aCore ) {
    UtilLogger::Info( "RENDERPASS", "Creating render pass." );

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = aCore.FindDepthFormat();
    depthAttachment.samples        = aCore.mySwapchainCtx.myMSAASamples;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments     = nullptr;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector< VkAttachmentDescription > attachments;
    attachments.reserve( 3 );
    const bool useMsaaResolve = ( aCore.mySwapchainCtx.myMSAASamples != VK_SAMPLE_COUNT_1_BIT );
    if ( useMsaaResolve ) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = aCore.mySwapchainCtx.mySwapChainImageFormat;
        colorAttachment.samples        = aCore.mySwapchainCtx.myMSAASamples;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format         = aCore.mySwapchainCtx.mySwapChainImageFormat;
        colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.pResolveAttachments          = &colorAttachmentResolveRef;

        attachments.push_back( colorAttachment );
        attachments.push_back( depthAttachment );
        attachments.push_back( colorAttachmentResolve );
    }
    else {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = aCore.mySwapchainCtx.mySwapChainImageFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments.push_back( colorAttachment );
        attachments.push_back( depthAttachment );
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if ( vkCreateRenderPass( aCore.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.mySwapchainCtx.myRenderPass ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create render pass!" );
    }

    // HybridDeferred: depth LOAD after G-buffer depth copy (ForwardTransparent reads opaque depth).
    // Depth attachment is always index 1 (color/MSAA color at 0; resolve at 2 when MSAA).
    std::vector< VkAttachmentDescription > hybridAttachments = attachments;
    if ( hybridAttachments.size() >= 2 ) {
        hybridAttachments[ 1 ].loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
        hybridAttachments[ 1 ].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        renderPassInfo.pAttachments          = hybridAttachments.data();
        if ( vkCreateRenderPass( aCore.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.mySwapchainCtx.myHybridResolveRenderPass ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create hybrid resolve render pass!" );
        }
    }

    const VkDevice     device       = aCore.myDeviceCtx.myDevice;
    const VkRenderPass render       = aCore.mySwapchainCtx.myRenderPass;
    const VkRenderPass hybridRender = aCore.mySwapchainCtx.myHybridResolveRenderPass;
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, render, hybridRender ]() {
        vkDestroyRenderPass( device, render, nullptr );
        if ( hybridRender != VK_NULL_HANDLE ) {
            vkDestroyRenderPass( device, hybridRender, nullptr );
        }
    } );
}

void Vk_SwapchainHost::CreateFrameBuffers( Vk_Core& aCore ) {
    const size_t imageCount = aCore.mySwapchainCtx.mySwapChainImageViews.size();
    aCore.mySwapchainCtx.mySwapChainFrameBuffers.resize( imageCount );
    aCore.mySwapchainCtx.myHybridSwapChainFrameBuffers.clear();
    if ( aCore.mySwapchainCtx.myHybridResolveRenderPass != VK_NULL_HANDLE ) {
        aCore.mySwapchainCtx.myHybridSwapChainFrameBuffers.resize( imageCount );
    }

    const bool useMsaaResolve = ( aCore.mySwapchainCtx.myMSAASamples != VK_SAMPLE_COUNT_1_BIT );
    for ( size_t i = 0; i < imageCount; i++ ) {
        std::vector< VkImageView > attachments;
        if ( useMsaaResolve ) {
            attachments = { aCore.mySwapchainCtx.myColorTexture.ImageView(), aCore.mySwapchainCtx.myDepthTexture.ImageView(),
                            aCore.mySwapchainCtx.mySwapChainImageViews[ i ] };
        }
        else {
            attachments = { aCore.mySwapchainCtx.mySwapChainImageViews[ i ], aCore.mySwapchainCtx.myDepthTexture.ImageView() };
        }

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
        frameBufferInfo.pAttachments    = attachments.data();
        frameBufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
        frameBufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
        frameBufferInfo.layers          = 1;

        frameBufferInfo.renderPass = aCore.mySwapchainCtx.myRenderPass;
        if ( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &frameBufferInfo, nullptr, &aCore.mySwapchainCtx.mySwapChainFrameBuffers[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create framebuffer!" );
        }

        if ( !aCore.mySwapchainCtx.myHybridSwapChainFrameBuffers.empty() ) {
            frameBufferInfo.renderPass = aCore.mySwapchainCtx.myHybridResolveRenderPass;
            if ( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &frameBufferInfo, nullptr, &aCore.mySwapchainCtx.myHybridSwapChainFrameBuffers[ i ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "failed to create hybrid resolve framebuffer!" );
            }
        }
    }

    const VkDevice device             = aCore.myDeviceCtx.myDevice;
    const auto     frameBuffers       = aCore.mySwapchainCtx.mySwapChainFrameBuffers;
    const auto     hybridFrameBuffers = aCore.mySwapchainCtx.myHybridSwapChainFrameBuffers;
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, frameBuffers, hybridFrameBuffers ]() {
        for ( VkFramebuffer frameBuffer : frameBuffers ) {
            vkDestroyFramebuffer( device, frameBuffer, nullptr );
        }
        for ( VkFramebuffer frameBuffer : hybridFrameBuffers ) {
            vkDestroyFramebuffer( device, frameBuffer, nullptr );
        }
    } );
}

void Vk_SwapchainHost::CreateDepthResources( Vk_Core& aCore ) {
    const VkFormat depthFormat = aCore.FindDepthFormat();
    aCore.CreateImage( aCore.mySwapchainCtx.mySwapChainExtent, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1, aCore.mySwapchainCtx.myMSAASamples,
                       aCore.mySwapchainCtx.myDepthTexture.AllocImage() );
    aCore.mySwapchainCtx.myDepthTexture.ImageView() = aCore.CreateImageView( aCore.mySwapchainCtx.myDepthTexture.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );
    aCore.TransitionImageLayout( aCore.mySwapchainCtx.myDepthTexture.Image(), depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1 );

    const VkDevice      device     = aCore.myDeviceCtx.myDevice;
    const VmaAllocator  allocator  = aCore.myDeviceCtx.myAllocator;
    const VkImageView   depthView  = aCore.mySwapchainCtx.myDepthTexture.ImageView();
    const VkImage       depthImage = aCore.mySwapchainCtx.myDepthTexture.Image();
    const VmaAllocation depthAlloc = aCore.mySwapchainCtx.myDepthTexture.Allocation();
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, allocator, depthView, depthImage, depthAlloc ]() {
        vkDestroyImageView( device, depthView, nullptr );
        vmaDestroyImage( allocator, depthImage, depthAlloc );
    } );
}

void Vk_SwapchainHost::CreateColorResources( Vk_Core& aCore ) {
    if ( aCore.mySwapchainCtx.myMSAASamples == VK_SAMPLE_COUNT_1_BIT ) {
        UtilLogger::Info( "RESOURCE", "Skipping MSAA color target (single-sample / direct swapchain)." );
        return;
    }

    const VkFormat colorFormat = aCore.mySwapchainCtx.mySwapChainImageFormat;
    aCore.CreateImage( aCore.mySwapchainCtx.mySwapChainExtent, colorFormat, VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1, aCore.mySwapchainCtx.myMSAASamples,
                       aCore.mySwapchainCtx.myColorTexture.AllocImage() );
    aCore.mySwapchainCtx.myColorTexture.ImageView() = aCore.CreateImageView( aCore.mySwapchainCtx.myColorTexture.Image(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT );

    const VkDevice      device     = aCore.myDeviceCtx.myDevice;
    const VmaAllocator  allocator  = aCore.myDeviceCtx.myAllocator;
    const VkImageView   colorView  = aCore.mySwapchainCtx.myColorTexture.ImageView();
    const VkImage       colorImage = aCore.mySwapchainCtx.myColorTexture.Image();
    const VmaAllocation colorAlloc = aCore.mySwapchainCtx.myColorTexture.Allocation();
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, allocator, colorView, colorImage, colorAlloc ]() {
        vkDestroyImageView( device, colorView, nullptr );
        vmaDestroyImage( allocator, colorImage, colorAlloc );
    } );
}

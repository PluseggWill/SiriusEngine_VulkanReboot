#include "Vk_SwapchainHost.h"

#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_GfxPipelineCache.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <vector>

namespace {

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

void Vk_SwapchainHost::Init( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Vk_SwapchainHost::Init." );
    CreateSwapChain( aCore );
    CreateRenderPass( aCore );
    CreateColorResources( aCore );
    CreateDepthResources( aCore );
    CreateFrameBuffers( aCore );
}

void Vk_SwapchainHost::Recreate( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Recreating swapchain." );
    int width = 0, height = 0;
    glfwGetFramebufferSize( aCore.myPlatformCtx.myWindow, &width, &height );
    while ( width == 0 || height == 0 ) {
        glfwGetFramebufferSize( aCore.myPlatformCtx.myWindow, &width, &height );
        glfwWaitEvents();
    }

    vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    aCore.myPlatformCtx.myImGuiLayer.DestroySwapchainResources();
    if ( aCore.HasLoadedScene() ) {
        Vk_GfxPipelineCache::DestroyScenePipelines( aCore );
    }
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.flush();

    CreateSwapChain( aCore );
    CreateRenderPass( aCore );
    if ( aCore.HasLoadedScene() ) {
        Vk_GfxPipelineCache::InitScenePipelines( aCore );
    }
    CreateColorResources( aCore );
    CreateDepthResources( aCore );
    CreateFrameBuffers( aCore );

    const uint32_t imageCount    = static_cast< uint32_t >( aCore.mySwapchainCtx.mySwapChainImageViews.size() );
    const uint32_t minImageCount = std::max( 2u, imageCount );
    aCore.myPlatformCtx.myImGuiLayer.CreateSwapchainResources( aCore.mySwapchainCtx.mySwapChainImageFormat, aCore.mySwapchainCtx.mySwapChainExtent, aCore.mySwapchainCtx.mySwapChainImageViews, imageCount, minImageCount );
    aCore.RefreshMaterialPipelinesAfterSwapchainRecreate();
    aCore.myCamera.SetAspect( static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.width ) / static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.height ) );
    UtilLogger::Info( "SWAPCHAIN", "Swapchain recreation completed." );
}

bool Vk_SwapchainHost::AcquireNextImage( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t& anOutImageIndex ) {
    static bool    sAcquirePathLogged = false;
    const VkResult result = vkAcquireNextImageKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, UINT64_MAX, aFrameData.myPresentSemaphore, VK_NULL_HANDLE, &anOutImageIndex );
    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        UtilLogger::Warn( "SWAPCHAIN", "Acquire image returned OUT_OF_DATE. Recreating swapchain." );
        Recreate( aCore );
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

Vk_FrameResult Vk_SwapchainHost::SubmitAndPresent( Vk_Core& aCore, const Vk_FrameData& aFrameData, uint32_t anImageIndex ) {
    static bool  sPresentPathLogged = false;
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

    const VkResult result = vkQueuePresentKHR( aCore.myDeviceCtx.myPresentQueue, &presentInfo );
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

void Vk_SwapchainHost::CreateSwapChain( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Creating swapchain." );
    const Vk_SwapChainSupportDetails swapChainSupport = aCore.QuerySwapChainSupport( aCore.myDeviceCtx.myPhysicalDevice );
    const VkSurfaceFormatKHR         surfaceFormat    = aCore.ChooseSwapSurfaceFormat( swapChainSupport.myFormats );
    const VkPresentModeKHR           presentMode      = aCore.ChooseSwapPresentMode( swapChainSupport.myPresentModes );
    const VkExtent2D                 extent           = aCore.ChooseSwapExtent( swapChainSupport.myCapabilities );
    uint32_t                         imageCount       = swapChainSupport.myCapabilities.minImageCount + 1;
    if ( swapChainSupport.myCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.myCapabilities.maxImageCount ) {
        imageCount = swapChainSupport.myCapabilities.maxImageCount;
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
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    if ( vkCreateSwapchainKHR( aCore.myDeviceCtx.myDevice, &createInfo, nullptr, &aCore.mySwapchainCtx.mySwapChain ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create swap chain!" );
    }
    vkGetSwapchainImagesKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, &imageCount, nullptr );
    aCore.mySwapchainCtx.mySwapChainImages.resize( imageCount );
    vkGetSwapchainImagesKHR( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.mySwapChain, &imageCount, aCore.mySwapchainCtx.mySwapChainImages.data() );
    aCore.mySwapchainCtx.mySwapChainImageFormat = surfaceFormat.format;
    aCore.mySwapchainCtx.mySwapChainExtent      = extent;
    aCore.mySwapchainCtx.mySwapChainImageViews.resize( imageCount );
    for ( size_t i = 0; i < imageCount; ++i ) {
        aCore.mySwapchainCtx.mySwapChainImageViews[ i ] = aCore.CreateImageView( aCore.mySwapchainCtx.mySwapChainImages[ i ], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT );
    }
    const VkDevice       device     = aCore.myDeviceCtx.myDevice;
    const VkSwapchainKHR swapchain  = aCore.mySwapchainCtx.mySwapChain;
    const auto           imageViews = aCore.mySwapchainCtx.mySwapChainImageViews;
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

    const VkDevice     device = aCore.myDeviceCtx.myDevice;
    const VkRenderPass render = aCore.mySwapchainCtx.myRenderPass;
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, render ]() { vkDestroyRenderPass( device, render, nullptr ); } );
}

void Vk_SwapchainHost::CreateFrameBuffers( Vk_Core& aCore ) {
    aCore.mySwapchainCtx.mySwapChainFrameBuffers.resize( aCore.mySwapchainCtx.mySwapChainImageViews.size() );
    const bool useMsaaResolve = ( aCore.mySwapchainCtx.myMSAASamples != VK_SAMPLE_COUNT_1_BIT );
    for ( size_t i = 0; i < aCore.mySwapchainCtx.mySwapChainImageViews.size(); i++ ) {
        std::vector< VkImageView > attachments;
        if ( useMsaaResolve ) {
            attachments = { aCore.mySwapchainCtx.myColorTexture.ImageView(), aCore.mySwapchainCtx.myDepthTexture.ImageView(), aCore.mySwapchainCtx.mySwapChainImageViews[ i ] };
        }
        else {
            attachments = { aCore.mySwapchainCtx.mySwapChainImageViews[ i ], aCore.mySwapchainCtx.myDepthTexture.ImageView() };
        }

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass      = aCore.mySwapchainCtx.myRenderPass;
        frameBufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
        frameBufferInfo.pAttachments    = attachments.data();
        frameBufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
        frameBufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
        frameBufferInfo.layers          = 1;
        if ( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &frameBufferInfo, nullptr, &aCore.mySwapchainCtx.mySwapChainFrameBuffers[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create framebuffer!" );
        }
    }

    const VkDevice device       = aCore.myDeviceCtx.myDevice;
    const auto     frameBuffers = aCore.mySwapchainCtx.mySwapChainFrameBuffers;
    aCore.mySwapchainCtx.mySwapChainDeletionQueue.pushFunction( [ device, frameBuffers ]() {
        for ( VkFramebuffer frameBuffer : frameBuffers ) {
            vkDestroyFramebuffer( device, frameBuffer, nullptr );
        }
    } );
}

void Vk_SwapchainHost::CreateDepthResources( Vk_Core& aCore ) {
    const VkFormat depthFormat = aCore.FindDepthFormat();
    aCore.CreateImage( aCore.mySwapchainCtx.mySwapChainExtent, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       aCore.mySwapchainCtx.myMSAASamples, aCore.mySwapchainCtx.myDepthTexture.AllocImage() );
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
    aCore.CreateImage( aCore.mySwapchainCtx.mySwapChainExtent, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY, 1, aCore.mySwapchainCtx.myMSAASamples, aCore.mySwapchainCtx.myColorTexture.AllocImage() );
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

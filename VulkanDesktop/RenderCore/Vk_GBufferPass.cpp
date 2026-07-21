// Module: Vk_GBufferPass — FG v1 (HybridDeferred preset).
// Chain driven by Vk_FrameGraph: Shadow -> GBuffer -> Cluster -> HiZ/SSAO -> HDR hybrid -> Post.
// G-buffer format: Docs/Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md (not EngineArchitecture until locked).
#include "Vk_GBufferPass.h"

#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_FramePacketValidation.h"
#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_RenderPipeline.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"
#include "Vk_AoPass.h"
#include "Vk_ClusterBuildPass.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_FrameGraph.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_GfxPipelineCache.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_ScenePasses.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_ShadowMapPass.h"

#include "Vk_VertexLayout.h"

#include <array>
#include <glm/glm.hpp>

namespace {

constexpr const char* kGBufferVertSpv         = "VulkanDesktop/Shader_Generated/GBufferVert.spv";
constexpr const char* kGBufferFragSpv         = "VulkanDesktop/Shader_Generated/GBufferFrag.spv";
constexpr const char* kGBufferFragBindlessSpv = "VulkanDesktop/Shader_Generated/GBufferFrag_Bindless.spv";

constexpr VkFormat kAlbedoFormat          = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kNormalRoughnessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kWorldPositionFormat   = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kMotionVectorFormat    = VK_FORMAT_R16G16_SFLOAT;

void DestroyPipelines( Vk_Renderer& aCore ) {
    const VkDevice device = aCore.myRhi.myDeviceCtx.myDevice;
    if ( device == VK_NULL_HANDLE ) {
        return;
    }
    if ( aCore.myGBufferState.myGBufferPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myGBufferState.myGBufferPipeline, nullptr );
        aCore.myGBufferState.myGBufferPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myGBufferState.myGBufferPipelineBindless != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myGBufferState.myGBufferPipelineBindless, nullptr );
        aCore.myGBufferState.myGBufferPipelineBindless = VK_NULL_HANDLE;
    }
}

VkPipeline BuildGBufferPipeline( Vk_Renderer& aCore, VkRenderPass aRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Vk_GetGfxVertexBindingDescription();
    auto                                 attributeDescription = Vk_GetGfxVertexAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo = vertexInputInfo;
    pipelineBuilder.myInputAssembly   = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport        = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    pipelineBuilder.myScissor.offset  = { 0, 0 };
    pipelineBuilder.myScissor.extent  = aCore.mySwapchainCtx.mySwapChainExtent;
    pipelineBuilder.myRasterizer      = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL );
    pipelineBuilder.myMultisampling   = VkInit::Pipeline_MultisampleCreateInfo( VK_SAMPLE_COUNT_1_BIT );
    pipelineBuilder.myDepthStencil    = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myPipelineLayout  = aLayout;
    pipelineBuilder.SetDefaultDynamicStates();

    std::vector< VkPipelineColorBlendAttachmentState > blendAttachments = {
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = VkInit::Pipeline_ColorBlendCreateInfo( blendAttachments );

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &pipelineBuilder.myViewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &pipelineBuilder.myScissor;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast< uint32_t >( pipelineBuilder.myShaderStages.size() );
    pipelineInfo.pStages             = pipelineBuilder.myShaderStages.data();
    pipelineInfo.pVertexInputState   = &pipelineBuilder.myVertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineBuilder.myInputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &pipelineBuilder.myRasterizer;
    pipelineInfo.pMultisampleState   = &pipelineBuilder.myMultisampling;
    pipelineInfo.pDepthStencilState  = &pipelineBuilder.myDepthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &pipelineBuilder.myDynamicState;
    pipelineInfo.layout              = aLayout;
    pipelineInfo.renderPass          = aRenderPass;
    pipelineInfo.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    UtilVulkanResult::ThrowOnFailure(
        vkCreateGraphicsPipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ),
        "vkCreateGraphicsPipelines GBuffer" );
    UtilLogger::Info( "PIPELINE", "GBuffer pipeline created." );

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
}

void CreateGBufferRenderPass( Vk_Renderer& aCore ) {
    VkAttachmentDescription albedo{};
    albedo.format         = kAlbedoFormat;
    albedo.samples        = VK_SAMPLE_COUNT_1_BIT;
    albedo.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedo.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    albedo.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedo.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    albedo.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalRoughness = albedo;
    normalRoughness.format                  = kNormalRoughnessFormat;

    VkAttachmentDescription worldPosition = albedo;
    worldPosition.format                  = kWorldPositionFormat;
    VkAttachmentDescription motionVector  = albedo;
    motionVector.format                   = kMotionVectorFormat;

    VkAttachmentDescription depth{};
    depth.format         = aCore.FindDepthFormat();
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array< VkAttachmentReference, 4 > colorRefs{};
    colorRefs[ 0 ].attachment = 0;
    colorRefs[ 0 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[ 1 ].attachment = 1;
    colorRefs[ 1 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[ 2 ].attachment = 2;
    colorRefs[ 2 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[ 3 ].attachment = 3;
    colorRefs[ 3 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 4;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = static_cast< uint32_t >( colorRefs.size() );
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array< VkAttachmentDescription, 5 > attachments = { albedo, normalRoughness, worldPosition, motionVector, depth };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    UtilVulkanResult::ThrowOnFailure( vkCreateRenderPass( aCore.myRhi.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.myGBufferState.myRenderPass ),
                                      "vkCreateRenderPass GBuffer" );

    const VkDevice     device     = aCore.myRhi.myDeviceCtx.myDevice;
    const VkRenderPass renderPass = aCore.myGBufferState.myRenderPass;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, renderPass ]() { vkDestroyRenderPass( device, renderPass, nullptr ); } );
}

void CreateGBufferImages( Vk_Renderer& aCore ) {
    const VkExtent2D extent      = aCore.mySwapchainCtx.mySwapChainExtent;
    const VkFormat   depthFormat = aCore.FindDepthFormat();

    auto createColorTarget = [ & ]( Vk_TextureResource& aTexture, VkFormat aFormat ) {
        aCore.CreateImage( extent, aFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                           VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
        aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT );

        const VkDevice      device    = aCore.myRhi.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myRhi.myDeviceCtx.myAllocator;
        const VkImageView   view      = aTexture.ImageView();
        const VkImage       image     = aTexture.Image();
        const VmaAllocation alloc     = aTexture.Allocation();
        aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, allocator, view, image, alloc ]() {
            vkDestroyImageView( device, view, nullptr );
            vmaDestroyImage( allocator, image, alloc );
        } );
    };

    createColorTarget( aCore.myGBufferState.myAlbedo, kAlbedoFormat );
    createColorTarget( aCore.myGBufferState.myNormalRoughness, kNormalRoughnessFormat );
    createColorTarget( aCore.myGBufferState.myWorldPosition, kWorldPositionFormat );
    createColorTarget( aCore.myGBufferState.myMotionVector, kMotionVectorFormat );

    aCore.CreateImage( extent, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aCore.myGBufferState.myDepth.AllocImage() );
    aCore.myGBufferState.myDepth.ImageView() = aCore.CreateImageView( aCore.myGBufferState.myDepth.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );
    aCore.myGBufferState.myDepthLayout       = VK_IMAGE_LAYOUT_UNDEFINED;

    {
        const VkDevice      device    = aCore.myRhi.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myRhi.myDeviceCtx.myAllocator;
        const VkImageView   view      = aCore.myGBufferState.myDepth.ImageView();
        const VkImage       image     = aCore.myGBufferState.myDepth.Image();
        const VmaAllocation alloc     = aCore.myGBufferState.myDepth.Allocation();
        aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, allocator, view, image, alloc ]() {
            vkDestroyImageView( device, view, nullptr );
            vmaDestroyImage( allocator, image, alloc );
        } );
    }
}

void CreateGBufferFramebuffer( Vk_Renderer& aCore ) {
    std::array< VkImageView, 5 > attachments = { aCore.myGBufferState.myAlbedo.ImageView(), aCore.myGBufferState.myNormalRoughness.ImageView(),
                                                 aCore.myGBufferState.myWorldPosition.ImageView(), aCore.myGBufferState.myMotionVector.ImageView(),
                                                 aCore.myGBufferState.myDepth.ImageView() };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = aCore.myGBufferState.myRenderPass;
    framebufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
    framebufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
    framebufferInfo.layers          = 1;

    UtilVulkanResult::ThrowOnFailure( vkCreateFramebuffer( aCore.myRhi.myDeviceCtx.myDevice, &framebufferInfo, nullptr, &aCore.myGBufferState.myFramebuffer ),
                                      "vkCreateFramebuffer GBuffer" );

    const VkDevice      device      = aCore.myRhi.myDeviceCtx.myDevice;
    const VkFramebuffer framebuffer = aCore.myGBufferState.myFramebuffer;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, framebuffer ]() { vkDestroyFramebuffer( device, framebuffer, nullptr ); } );
}

VkImageMemoryBarrier DepthImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    return barrier;
}

// Bind Set 0 (+ Set 1 when bindless). Re-bind after DeferredLighting (different pipeline layout).
void BindHybridSceneDescriptors( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, VkPipelineLayout aFrameBindLayout, VkDescriptorSet aFrameDescriptor, bool aBindless ) {
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aFrameBindLayout, VkDescriptorPolicy::kSetFrame, 1, &aFrameDescriptor, 0, nullptr );
    if ( aBindless ) {
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                 &aCore.mySceneGpuCtx.myBindlessDescriptorSet, 0, nullptr );
        aCore.myFrameStats.myMaterialSetBinds++;
    }
}

VkImageMemoryBarrier ColorImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return barrier;
}

// G-buffer MRT colors must be shader-readable before DeferredLighting (depth is handled by CmdCopyGBufferDepthToSwapchain).
void CmdBarrierGBufferColorsForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    constexpr VkImageLayout kReadLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array< VkImageMemoryBarrier, 4 > barriers = {
        ColorImageBarrier( aCore.myGBufferState.myAlbedo.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myNormalRoughness.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myWorldPosition.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myMotionVector.Image(), kReadLayout, kReadLayout, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( barriers.size() ), barriers.data() );
}

// G-buffer depth: attachment -> shader read (Hi-Z / SSAO / deferred reconstruct before swapchain depth copy).
void CmdBarrierGBufferDepthForShaderRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    constexpr VkImageLayout kReadLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkImageLayout&          layout      = aCore.myGBufferState.myDepthLayout;
    if ( layout == kReadLayout ) {
        return;
    }
    const VkImageLayout  oldLayout = ( layout == VK_IMAGE_LAYOUT_UNDEFINED ) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : layout;
    VkImageMemoryBarrier barrier =
        DepthImageBarrier( aCore.myGBufferState.myDepth.Image(), oldLayout, kReadLayout, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &barrier );
    layout = kReadLayout;
}

// Shadow pass depth must be visible to DeferredLighting fragment shader (separate render pass from hybrid resolve).
void CmdBarrierShadowMapForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    Vk_ShadowMapPass::CmdBarrierForDeferredRead( aCore, aCommandBuffer );
}

// CONTRACT: run outside swapchain RP; dst depth must be TRANSFER_DST + attachment usage.
void CmdCopyGBufferDepthToSwapchain( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    VkImage          srcImage = aCore.myGBufferState.myDepth.Image();
    VkImage          dstImage = aCore.mySwapchainCtx.myDepthTexture.Image();
    const VkExtent2D extent   = aCore.mySwapchainCtx.mySwapChainExtent;

    constexpr VkImageLayout kDepthReadOnly = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array< VkImageMemoryBarrier, 2 > toTransfer = {
        DepthImageBarrier( srcImage, kDepthReadOnly, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT ),
        DepthImageBarrier( dstImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT ),
    };
    VkImageMemoryBarrier srcToTransfer = toTransfer[ 0 ];
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, 1, &srcToTransfer );
    VkImageMemoryBarrier dstToTransfer = toTransfer[ 1 ];
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &dstToTransfer );

    VkImageCopy region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource            = region.srcSubresource;
    region.extent                    = { extent.width, extent.height, 1 };
    vkCmdCopyImage( aCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    // Src stays shader-readable for DeferredLighting depth reconstruct; dst is swapchain depth for transparent pass.
    std::array< VkImageMemoryBarrier, 2 > toAttachment = {
        DepthImageBarrier( srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_SHADER_READ_BIT ),
        DepthImageBarrier( dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toAttachment.size() ), toAttachment.data() );
}

void RebuildGBufferPipelines( Vk_Renderer& aCore ) {
    if ( aCore.myGBufferState.myRenderPass == VK_NULL_HANDLE || aCore.mySceneGpuCtx.myPipelineLayout == VK_NULL_HANDLE ) {
        return;
    }

    DestroyPipelines( aCore );

    const std::string vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferVertSpv );
    const std::string fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferFragSpv );

    aCore.myGBufferState.myGBufferPipeline = BuildGBufferPipeline( aCore, aCore.myGBufferState.myRenderPass, aCore.mySceneGpuCtx.myPipelineLayout, vertPath, fragPath );

    if ( aCore.myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless && aCore.mySceneGpuCtx.myBindlessPipelineLayout != VK_NULL_HANDLE ) {
        const std::string bindlessFragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferFragBindlessSpv );
        aCore.myGBufferState.myGBufferPipelineBindless =
            BuildGBufferPipeline( aCore, aCore.myGBufferState.myRenderPass, aCore.mySceneGpuCtx.myBindlessPipelineLayout, vertPath, bindlessFragPath );
    }
}

void RebuildResources( Vk_Renderer& aCore ) {
    if ( aCore.myRhi.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }

    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();

    CreateGBufferRenderPass( aCore );
    CreateGBufferImages( aCore );
    CreateGBufferFramebuffer( aCore );

    RebuildGBufferPipelines( aCore );
}

}  // namespace

namespace Vk_GBufferPass {

bool IsActive( const Vk_Renderer& aCore ) {
    return aCore.RenderFeatures().myHybridDeferred;
}

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();
    aCore.myGBufferState.myFramebuffer = VK_NULL_HANDLE;
    aCore.myGBufferState.myRenderPass  = VK_NULL_HANDLE;
    aCore.myGBufferState.myDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    aCore.myGBufferState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    // ForwardLit never calls Init — skip swapchain resize work when hybrid path is inactive.
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    RebuildResources( aCore );
    Vk_DepthPyramidPass::RecreateForExtent( aCore );
    Vk_SsrPass::RecreateForExtent( aCore );
    Vk_AoPass::RecreateForExtent( aCore );
    Vk_ShadowAoSoftPass::RecreateForExtent( aCore );
    Vk_PostProcessPass::RecreateForExtent( aCore );
    Vk_DeferredLightingPass::RecreateForExtent( aCore );
    Vk_GfxPipelineCache::CreateHybridResolveGfxPipelines( aCore );
}

void RecreatePipelines( Vk_Renderer& aCore ) {
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    RebuildGBufferPipelines( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myGBufferState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_GBufferPass::Init." );
    RebuildResources( aCore );
    aCore.myGBufferState.myInitialized = true;
}

void RecordFrame( Vk_Renderer& aCore, const Gfx_FrameDebugToggles& aToggles, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {

    static bool sIndirectPathLoggedOnce    = false;
    static bool sGpuIndirectPathLoggedOnce = false;

    Vk_FrameGraphContext fgCtx{};
    fgCtx.myCore             = &aCore;
    fgCtx.myToggles          = &aToggles;
    fgCtx.myCommandBuffer    = aCommandBuffer;
    fgCtx.myImageIndex       = anImageIndex;
    fgCtx.myFrameIndex       = aCore.myFrameCtx.myCurrentFrame;
    fgCtx.myViewports        = &aViewports;
    fgCtx.myScissors         = &aScissors;
    fgCtx.myFrameDescriptors = &aFrameDescriptors;
    fgCtx.myViewCount        = aViewCount;
    fgCtx.myViewPackets      = &aViewPackets;

    Gfx_PipelineBuildInput buildInput{};
    buildInput.myLighting                  = aCore.myLightingSettings;
    buildInput.myAo                        = aCore.myAoSettings;
    buildInput.mySunDirectionTowardLight   = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    buildInput.myReady.myShadowMap         = aCore.myShadowMapState.myInitialized;
    buildInput.myReady.myDepthPyramid      = aCore.myDepthPyramidState.myInitialized;
    buildInput.myReady.mySsr               = aCore.mySsrState.myInitialized;
    buildInput.myReady.myAo                = aCore.myAoState.myInitialized;
    buildInput.myReady.myShadowAoSoft      = aCore.myShadowAoSoftState.myInitialized;
    buildInput.myReady.myDeferredLighting  = aCore.myDeferredLightingState.myInitialized;
    buildInput.myReady.myPostHybridResolve = Vk_PostProcessPass::HasHybridResolve( aCore );
    fgCtx.myEnable                         = Gfx_RenderPipeline::ResolveEnableFlags( buildInput );

    Vk_FrameGraph::Execute( fgCtx );

    constexpr uint32_t           viewIndex        = 0;
    const Gfx_FrameRenderPacket* packet           = viewIndex < aViewCount ? &aViewPackets[ viewIndex ] : nullptr;
    const bool                   legacyDirectDraw = aToggles.myLegacyDirectDraw;
    const bool                   gpuCullRecord    = aToggles.myGpuCullEnabled && !legacyDirectDraw;

    if ( aViewCount > 0 && packet != nullptr && gpuCullRecord && !sGpuIndirectPathLoggedOnce ) {
        UtilLogger::Info( "RENDER", "Scene record using GPU-filled slot indirect (EntityCull.comp → myGpuCullIndirectBuffer)." );
        sGpuIndirectPathLoggedOnce = true;
    }

    if ( aViewCount > 0 && packet != nullptr && !legacyDirectDraw && !gpuCullRecord && !sIndirectPathLoggedOnce ) {
        UtilLogger::Info( "RENDER", "Scene record using CPU vkCmdDrawIndexedIndirect (draw templates uploaded each frame)." );
        sIndirectPathLoggedOnce = true;
    }
}

}  // namespace Vk_GBufferPass

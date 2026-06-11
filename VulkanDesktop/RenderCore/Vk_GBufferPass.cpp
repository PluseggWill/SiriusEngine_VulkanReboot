// Module: Vk_GBufferPass — FG v0 (HybridDeferred preset).
// Chain: GBufferOpaque -> ClusterBuild -> DeferredLighting -> swapchain.
// G-buffer format: Docs/Archived/plans/s3-fg-s1-preset-gbuffer_Plan.md (not EngineArchitecture until locked).
#include "Vk_GBufferPass.h"

#include "../App/DebugUIState.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_ClusterBuildPass.h"
#include "Vk_Core.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_RenderBackend.h"
#include "Vk_ScenePasses.h"

#include "Vk_Types.h"

#include <array>

namespace {

constexpr const char* kGBufferVertSpv = "VulkanDesktop/Shader_Generated/GBufferVert.spv";
constexpr const char* kGBufferFragSpv = "VulkanDesktop/Shader_Generated/GBufferFrag.spv";

constexpr VkFormat kAlbedoFormat          = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kNormalRoughnessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

void DestroyPipelines( Vk_Core& aCore ) {
    const VkDevice device = aCore.myDeviceCtx.myDevice;
    if ( device == VK_NULL_HANDLE ) {
        return;
    }
    if ( aCore.myGBufferState.myGBufferPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myGBufferState.myGBufferPipeline, nullptr );
        aCore.myGBufferState.myGBufferPipeline = VK_NULL_HANDLE;
    }
}

VkPipeline BuildGBufferPipeline( Vk_Core& aCore, VkRenderPass aRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
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
    UtilVulkanResult::ThrowOnFailure( vkCreateGraphicsPipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ),
                                      "vkCreateGraphicsPipelines GBuffer" );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
}

void CreateGBufferRenderPass( Vk_Core& aCore ) {
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

    VkAttachmentDescription depth{};
    depth.format         = aCore.FindDepthFormat();
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array< VkAttachmentReference, 2 > colorRefs{};
    colorRefs[ 0 ].attachment = 0;
    colorRefs[ 0 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[ 1 ].attachment = 1;
    colorRefs[ 1 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 2;
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

    std::array< VkAttachmentDescription, 3 > attachments = { albedo, normalRoughness, depth };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    UtilVulkanResult::ThrowOnFailure( vkCreateRenderPass( aCore.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.myGBufferState.myRenderPass ),
                                      "vkCreateRenderPass GBuffer" );

    const VkDevice     device     = aCore.myDeviceCtx.myDevice;
    const VkRenderPass renderPass = aCore.myGBufferState.myRenderPass;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, renderPass ]() { vkDestroyRenderPass( device, renderPass, nullptr ); } );
}

void CreateGBufferImages( Vk_Core& aCore ) {
    const VkExtent2D extent      = aCore.mySwapchainCtx.mySwapChainExtent;
    const VkFormat   depthFormat = aCore.FindDepthFormat();

    auto createColorTarget = [ & ]( Gfx_Texture& aTexture, VkFormat aFormat ) {
        aCore.CreateImage( extent, aFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                           VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
        aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT );

        const VkDevice      device    = aCore.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myDeviceCtx.myAllocator;
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

    aCore.CreateImage( extent, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT,
                       aCore.myGBufferState.myDepth.AllocImage() );
    aCore.myGBufferState.myDepth.ImageView() = aCore.CreateImageView( aCore.myGBufferState.myDepth.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );

    {
        const VkDevice      device    = aCore.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myDeviceCtx.myAllocator;
        const VkImageView   view      = aCore.myGBufferState.myDepth.ImageView();
        const VkImage       image     = aCore.myGBufferState.myDepth.Image();
        const VmaAllocation alloc     = aCore.myGBufferState.myDepth.Allocation();
        aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, allocator, view, image, alloc ]() {
            vkDestroyImageView( device, view, nullptr );
            vmaDestroyImage( allocator, image, alloc );
        } );
    }
}

void CreateGBufferFramebuffer( Vk_Core& aCore ) {
    std::array< VkImageView, 3 > attachments = { aCore.myGBufferState.myAlbedo.ImageView(), aCore.myGBufferState.myNormalRoughness.ImageView(),
                                                 aCore.myGBufferState.myDepth.ImageView() };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = aCore.myGBufferState.myRenderPass;
    framebufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
    framebufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
    framebufferInfo.layers          = 1;

    UtilVulkanResult::ThrowOnFailure( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &framebufferInfo, nullptr, &aCore.myGBufferState.myFramebuffer ),
                                      "vkCreateFramebuffer GBuffer" );

    const VkDevice      device      = aCore.myDeviceCtx.myDevice;
    const VkFramebuffer framebuffer = aCore.myGBufferState.myFramebuffer;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, framebuffer ]() { vkDestroyFramebuffer( device, framebuffer, nullptr ); } );
}

void RebuildResources( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }

    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();

    const std::string vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferVertSpv );
    const std::string fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferFragSpv );

    CreateGBufferRenderPass( aCore );
    CreateGBufferImages( aCore );
    CreateGBufferFramebuffer( aCore );

    aCore.myGBufferState.myGBufferPipeline = BuildGBufferPipeline( aCore, aCore.myGBufferState.myRenderPass, aCore.mySceneGpuCtx.myPipelineLayout, vertPath, fragPath );
}

}  // namespace

namespace Vk_GBufferPass {

bool IsActive( const Vk_Core& aCore ) {
    return Gfx_RenderPreset::IsHybridDeferred( aCore.EngineConfig().GetRenderPresetName() );
}

void Destroy( Vk_Core& aCore ) {
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();
    aCore.myGBufferState.myFramebuffer = VK_NULL_HANDLE;
    aCore.myGBufferState.myRenderPass  = VK_NULL_HANDLE;
    aCore.myGBufferState.myInitialized = false;
}

void RecreateForExtent( Vk_Core& aCore ) {
    // ForwardLit never calls Init — skip swapchain resize work when hybrid path is inactive.
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    RebuildResources( aCore );
}

void Init( Vk_Core& aCore ) {
    if ( aCore.myGBufferState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_GBufferPass::Init." );
    RebuildResources( aCore );
    aCore.myGBufferState.myInitialized = true;
}

void RecordFrame( Vk_Core& aCore, const DebugUIState& aDebugUI, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {

    static bool sChainLoggedOnce      = false;
    static bool sBindlessFallbackOnce = false;
    static bool sMultiViewWarnOnce    = false;
    static bool sTransparentSkipOnce  = false;

    if ( !sChainLoggedOnce ) {
        UtilLogger::Info( "FG", "HybridDeferred: GBufferOpaque -> ClusterBuild -> DeferredLighting" );
        sChainLoggedOnce = true;
    }

    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        if ( !sBindlessFallbackOnce ) {
            UtilLogger::Warn( "FG", "HybridDeferred is batch-only; falling back to ForwardLit record." );
            sBindlessFallbackOnce = true;
        }
        Vk_ScenePasses::RecordForwardLit( aCore, aDebugUI, aCommandBuffer, anImageIndex, aViewports, aScissors, aFrameDescriptors, aViewCount, aViewPackets );
        return;
    }

    if ( aViewCount > 1 && !sMultiViewWarnOnce ) {
        UtilLogger::Warn( "FG", "HybridDeferred FG v0 uses view 0 only." );
        sMultiViewWarnOnce = true;
    }

    if ( !sTransparentSkipOnce ) {
        UtilLogger::Info( "FG", "HybridDeferred FG v0: transparent pass skipped." );
        sTransparentSkipOnce = true;
    }

    const bool legacyDirectDraw = aCore.EngineConfig().GetLegacyDirectDraw();
    const bool gpuCullRecord    = aCore.EngineConfig().GetGpuCullEnabled() && !legacyDirectDraw;
    const bool emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();

    Vk_FrameData&  frame          = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
    const VkBuffer indirectBuffer = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;

    VkRenderPassBeginInfo gbufferBegin{};
    gbufferBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gbufferBegin.renderPass        = aCore.myGBufferState.myRenderPass;
    gbufferBegin.framebuffer       = aCore.myGBufferState.myFramebuffer;
    gbufferBegin.renderArea.offset = { 0, 0 };
    gbufferBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 3 > gbufferClears{};
    gbufferClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    gbufferClears[ 1 ].color        = { { 0.0f, 0.0f, 1.0f, 0.5f } };
    gbufferClears[ 2 ].depthStencil = { 1.0f, 0 };
    gbufferBegin.clearValueCount    = static_cast< uint32_t >( gbufferClears.size() );
    gbufferBegin.pClearValues       = gbufferClears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &gbufferBegin, VK_SUBPASS_CONTENTS_INLINE );

    const uint32_t viewIndex = 0;
    if ( viewIndex < aViewCount ) {
        const Gfx_FrameRenderPacket& packet = aViewPackets[ viewIndex ];
        aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex ], aScissors[ viewIndex ] );
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myPipelineLayout, VkDescriptorPolicy::kSetFrame, 1,
                                 &aFrameDescriptors[ viewIndex ], 0, nullptr );

        if ( Vk_RenderBackend::ValidateFramePacket( packet ) && !aDebugUI.myRenderDebug.mySkipOpaquePass ) {
            if ( emitDebugLabels ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=GBufferOpaque" );
            }
            Vk_ScenePasses::RecordOpaquePacketDraws( aCore, aCommandBuffer, packet.myOpaquePass, packet.myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord, legacyDirectDraw,
                                                     emitDebugLabels, aCore.myGBufferState.myGBufferPipeline );
            if ( emitDebugLabels ) {
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }
    }

    vkCmdEndRenderPass( aCommandBuffer );

    Vk_ClusterBuildPass::RecordDispatch( aCore, aCommandBuffer, aCore.myFrameCtx.myCurrentFrame );

    // Swapchain RP: fullscreen DeferredLighting (caller owns begin/end).
    VkRenderPassBeginInfo swapBegin{};
    swapBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    swapBegin.renderPass        = aCore.mySwapchainCtx.myRenderPass;
    swapBegin.framebuffer       = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ anImageIndex ];
    swapBegin.renderArea.offset = { 0, 0 };
    swapBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 2 > swapClears{};
    swapClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    swapClears[ 1 ].depthStencil = { 1.0f, 0 };
    swapBegin.clearValueCount    = static_cast< uint32_t >( swapClears.size() );
    swapBegin.pClearValues       = swapClears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &swapBegin, VK_SUBPASS_CONTENTS_INLINE );

    aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex < aViewCount ? viewIndex : 0 ], aScissors[ viewIndex < aViewCount ? viewIndex : 0 ] );
    Vk_DeferredLightingPass::RecordDraw( aCore, aCommandBuffer, aCore.myFrameCtx.myCurrentFrame );

    vkCmdEndRenderPass( aCommandBuffer );
}

}  // namespace Vk_GBufferPass

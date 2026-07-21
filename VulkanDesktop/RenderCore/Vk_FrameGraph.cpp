#include "Vk_FrameGraph.h"

#include "../Gfx/Gfx_FramePacketValidation.h"
#include "../Gfx/Gfx_RenderPipeline.h"
#include "../Util/Util_Logger.h"
#include "Vk_AoPass.h"
#include "Vk_ClusterBuildPass.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_FgBarrierCompiler.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_GBufferPass.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_ScenePasses.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_ShadowMapPass.h"
#include "Vk_SsrPass.h"

#include <algorithm>
#include <array>

namespace {

void BindHybridSceneDescriptors( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, VkPipelineLayout aFrameBindLayout, VkDescriptorSet aFrameDescriptor, bool aBindless ) {
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aFrameBindLayout, VkDescriptorPolicy::kSetFrame, 1, &aFrameDescriptor, 0, nullptr );
    if ( aBindless ) {
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                 &aCore.mySceneGpuCtx.myBindlessDescriptorSet, 0, nullptr );
        aCore.myFrameStats.myMaterialSetBinds++;
    }
}

void RecordShadow( Vk_FrameGraphContext& aCtx ) {
    constexpr uint32_t           viewIndex       = 0;
    const Gfx_FrameRenderPacket* packet          = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
    const bool                   emitDebugLabels = aCtx.myCore->AreCommandDebugLabelsEnabled();
    if ( packet != nullptr && !packet->myShadowCasterPass.myDraws.empty() ) {
        Vk_ShadowMapPass::RecordDraw( *aCtx.myCore, aCtx.myCommandBuffer, packet->myShadowCasterPass, emitDebugLabels );
    }
    else {
        Vk_ShadowMapPass::RecordDraw( *aCtx.myCore, aCtx.myCommandBuffer, Gfx_PassDrawPacket{}, emitDebugLabels );
    }
}

void RecordGBuffer( Vk_FrameGraphContext& aCtx ) {
    Vk_Renderer&                 aCore            = *aCtx.myCore;
    const Vk_RenderMaterialPath  materialPath     = aCore.myRhi.myDeviceCtx.myMaterialPath;
    const bool                   bindless         = materialPath == Vk_RenderMaterialPath::Bindless;
    const VkPipelineLayout       frameBindLayout  = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;
    const VkPipeline             gbufferPipeline  = bindless ? aCore.myGBufferState.myGBufferPipelineBindless : aCore.myGBufferState.myGBufferPipeline;
    const bool                   legacyDirectDraw = aCtx.myToggles->myLegacyDirectDraw;
    const bool                   gpuCullRecord    = aCtx.myToggles->myGpuCullEnabled && !legacyDirectDraw;
    const bool                   emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();
    Vk_FrameData&                frame            = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
    const VkBuffer               indirectBuffer   = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;
    constexpr uint32_t           viewIndex        = 0;
    const Gfx_FrameRenderPacket* packet           = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
    if ( !aCtx.myEnable.myShadow ) {
        Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( aCore, aCore.myFrameCtx.myCurrentFrame );
    }
    VkRenderPassBeginInfo gbufferBegin{};
    gbufferBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gbufferBegin.renderPass        = aCore.myGBufferState.myRenderPass;
    gbufferBegin.framebuffer       = aCore.myGBufferState.myFramebuffer;
    gbufferBegin.renderArea.offset = { 0, 0 };
    gbufferBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 5 > gbufferClears{};
    gbufferClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    gbufferClears[ 1 ].color        = { { 0.0f, 0.0f, 1.0f, 0.5f } };
    gbufferClears[ 2 ].color        = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    gbufferClears[ 3 ].color        = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    gbufferClears[ 4 ].depthStencil = { 1.0f, 0 };
    gbufferBegin.clearValueCount    = static_cast< uint32_t >( gbufferClears.size() );
    gbufferBegin.pClearValues       = gbufferClears.data();
    vkCmdBeginRenderPass( aCtx.myCommandBuffer, &gbufferBegin, VK_SUBPASS_CONTENTS_INLINE );
    if ( packet != nullptr && aCtx.myViewports != nullptr && aCtx.myScissors != nullptr && aCtx.myFrameDescriptors != nullptr ) {
        aCore.SetGraphicsDynamicState( aCtx.myCommandBuffer, ( *aCtx.myViewports )[ viewIndex ], ( *aCtx.myScissors )[ viewIndex ] );
        BindHybridSceneDescriptors( aCore, aCtx.myCommandBuffer, frameBindLayout, ( *aCtx.myFrameDescriptors )[ viewIndex ], bindless );
        if ( gbufferPipeline != VK_NULL_HANDLE && Gfx_FramePacketValidation::ValidateFramePacket( *packet ) && !aCtx.myToggles->mySkipOpaquePass ) {
            if ( emitDebugLabels )
                aCore.CmdBeginDebugLabel( aCtx.myCommandBuffer, "Pass=GBufferOpaque" );
            Vk_ScenePasses::RecordOpaquePacketDraws( aCore, aCtx.myCommandBuffer, packet->myOpaquePass, packet->myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord,
                                                     legacyDirectDraw, emitDebugLabels, gbufferPipeline );
            if ( emitDebugLabels )
                aCore.CmdEndDebugLabel( aCtx.myCommandBuffer );
        }
    }
    vkCmdEndRenderPass( aCtx.myCommandBuffer );
    aCore.myGBufferState.myDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void RecordDeferredTransparent( Vk_FrameGraphContext& aCtx ) {
    Vk_Renderer& aCore = *aCtx.myCore;
    Vk_ShadowMapPass::CmdBarrierForDeferredRead( aCore, aCtx.myCommandBuffer );
    Vk_FgBarrierCompiler::CmdCopyGBufferDepthToSwapchain( aCore, aCtx.myCommandBuffer );
    VkRenderPassBeginInfo hybridBegin{};
    hybridBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    hybridBegin.renderPass        = aCore.myPostProcessState.myHybridRenderPass;
    hybridBegin.framebuffer       = aCore.myPostProcessState.myHybridFramebuffer;
    hybridBegin.renderArea.offset = { 0, 0 };
    hybridBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 2 > hybridClears{};
    hybridClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    hybridClears[ 1 ].depthStencil = { 1.0f, 0 };
    hybridBegin.clearValueCount    = static_cast< uint32_t >( hybridClears.size() );
    hybridBegin.pClearValues       = hybridClears.data();
    vkCmdBeginRenderPass( aCtx.myCommandBuffer, &hybridBegin, VK_SUBPASS_CONTENTS_INLINE );
    const Vk_RenderMaterialPath  materialPath     = aCore.myRhi.myDeviceCtx.myMaterialPath;
    const bool                   bindless         = materialPath == Vk_RenderMaterialPath::Bindless;
    const VkPipelineLayout       frameBindLayout  = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;
    const bool                   legacyDirectDraw = aCtx.myToggles->myLegacyDirectDraw;
    const bool                   gpuCullRecord    = aCtx.myToggles->myGpuCullEnabled && !legacyDirectDraw;
    const bool                   emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();
    Vk_FrameData&                frame            = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
    const VkBuffer               indirectBuffer   = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;
    constexpr uint32_t           viewIndex        = 0;
    const Gfx_FrameRenderPacket* packet           = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
    if ( aCtx.myViewports != nullptr && aCtx.myScissors != nullptr ) {
        aCore.SetGraphicsDynamicState( aCtx.myCommandBuffer, ( *aCtx.myViewports )[ viewIndex ], ( *aCtx.myScissors )[ viewIndex ] );
    }
    Vk_DeferredLightingPass::RecordDraw( aCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
    if ( packet != nullptr && aCtx.myFrameDescriptors != nullptr && Gfx_FramePacketValidation::ValidateFramePacket( *packet ) && !aCtx.myToggles->mySkipTransparentPass
         && !packet->myTransparentPass.myDraws.empty() ) {
        BindHybridSceneDescriptors( aCore, aCtx.myCommandBuffer, frameBindLayout, ( *aCtx.myFrameDescriptors )[ viewIndex ], bindless );
        if ( emitDebugLabels )
            aCore.CmdBeginDebugLabel( aCtx.myCommandBuffer, "Pass=ForwardTransparent" );
        Vk_ScenePasses::RecordTransparentPacketDraws( aCore, aCtx.myCommandBuffer, packet->myTransparentPass, packet->myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord,
                                                      legacyDirectDraw, emitDebugLabels );
        if ( emitDebugLabels )
            aCore.CmdEndDebugLabel( aCtx.myCommandBuffer );
    }
    if ( aCtx.myViewports != nullptr && aCtx.myScissors != nullptr && aCtx.myFrameDescriptors != nullptr && aCtx.myViewPackets != nullptr ) {
        Vk_ScenePasses::RecordHybridPiPViews( aCore, *aCtx.myToggles, aCtx.myCommandBuffer, *aCtx.myViewports, *aCtx.myScissors, *aCtx.myFrameDescriptors, aCtx.myViewCount,
                                              *aCtx.myViewPackets );
    }
    vkCmdEndRenderPass( aCtx.myCommandBuffer );
    Vk_PostProcessPass::MarkSceneColorShaderRead();
    Vk_SsrPass::RecordHistoryUpdate( aCore, aCtx.myCommandBuffer );
}

void RecordPass( Gfx_PassId aPassId, Vk_FrameGraphContext& aCtx ) {
    switch ( aPassId ) {
    case Gfx_PassId::Shadow:
        RecordShadow( aCtx );
        break;
    case Gfx_PassId::GBuffer:
        RecordGBuffer( aCtx );
        break;
    case Gfx_PassId::ClusterBuild:
        Vk_ClusterBuildPass::RecordDispatch( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::DepthPyramid:
        Vk_FgBarrierCompiler::CmdBarrierGBufferColorsForDeferredRead( *aCtx.myCore, aCtx.myCommandBuffer );
        Vk_FgBarrierCompiler::CmdBarrierGBufferDepthForShaderRead( *aCtx.myCore, aCtx.myCommandBuffer );
        Vk_DepthPyramidPass::RecordBuild( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::SSR:
        Vk_FgBarrierCompiler::CmdBarrierGBufferColorsForDeferredRead( *aCtx.myCore, aCtx.myCommandBuffer );
        Vk_FgBarrierCompiler::CmdBarrierGBufferDepthForShaderRead( *aCtx.myCore, aCtx.myCommandBuffer );
        Vk_SsrPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::SSAO:
        Vk_AoPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::DdgiProbeUpdate:
        Vk_DeferredLightingPass::RecordDdgiProbeUpdate( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::ShadowAoSoft:
        Vk_ShadowAoSoftPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex, aCtx.myEnable.myAo );
        break;
    case Gfx_PassId::DeferredTransparent:
        RecordDeferredTransparent( aCtx );
        break;
    case Gfx_PassId::Post:
        Vk_PostProcessPass::RecordPost( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myImageIndex, aCtx.myFrameIndex );
        break;
    case Gfx_PassId::Count:
        break;
    }
}

}  // namespace

namespace Vk_FrameGraph {

void Execute( Vk_FrameGraphContext& aCtx ) {
    static bool sChainLoggedOnce = false;
    if ( !sChainLoggedOnce ) {
        UtilLogger::Info( "FG", Gfx_RenderPipeline::HybridDeferredChainLabel() );
        sChainLoggedOnce = true;
    }

    const Gfx_FramePlan plan = Gfx_RenderPipeline::BuildHybridDeferred( aCtx.myEnable );
    for ( Gfx_PassId passId : plan.myOrdered ) {
        const auto nodeIt = std::find_if( plan.myNodes.begin(), plan.myNodes.end(), [ passId ]( const Gfx_FramePlanNode& aNode ) { return aNode.myId == passId; } );
        if ( nodeIt == plan.myNodes.end() || !nodeIt->myEnabled ) {
            continue;
        }
        RecordPass( passId, aCtx );
    }
}

}  // namespace Vk_FrameGraph

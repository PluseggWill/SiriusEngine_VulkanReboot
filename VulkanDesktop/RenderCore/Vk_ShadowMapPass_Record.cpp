// Module: Vk_ShadowMapPass record path — thin facade over Gfx_ShadowMapPass::Record (Rhi).
#include "Vk_ShadowMapPass.h"

#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_ShadowMapPass.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <vector>

namespace {

// Mirror of anonymous helpers in Vk_ShadowMapPass.cpp for depth transitions used before Gfx RP.
VkImageMemoryBarrier MakeShadowDepthBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
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

void CmdTransitionShadowDepth( Vk_ShadowMapState& aState, VkCommandBuffer aCommandBuffer, VkImageLayout aNewLayout, VkPipelineStageFlags aDstStageMask,
                               VkAccessFlags aDstAccess ) {
    if ( aState.myDepthLayout == aNewLayout && aDstAccess == 0 ) {
        return;
    }

    VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags        srcAccess = 0;
    if ( aState.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
        srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        srcAccess = VK_ACCESS_SHADER_READ_BIT;
    }
    else if ( aState.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        srcStage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    const VkImageMemoryBarrier barrier = MakeShadowDepthBarrier( aState.myDepth.Image(), aState.myDepthLayout, aNewLayout, srcAccess, aDstAccess );
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, aDstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    aState.myDepthLayout = aNewLayout;
}

}  // namespace

namespace Vk_ShadowMapPass {

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aCasterPass, bool aEmitDebugLabels ) {
    if ( !aCore.myShadowMapState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    if ( !Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aCore.myLightingSettings.myShadowsEnabled, sunDir ) ) {
        Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( aCore, aCore.myFrameCtx.myCurrentFrame );
        return;
    }

    const Gfx_Bounds                                   sceneBounds = aCore.GetShadowCasterBounds();
    const Gfx_LightingMath::Gfx_DirectionalShadowSetup shadowSetup =
        Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowSetup( sunDir, sceneBounds, Vk_ShadowMapState::kMapSize );

    Vk_ShadowMapState& state  = aCore.myShadowMapState;
    state.myLightViewProj     = shadowSetup.myLightViewProj;
    state.myDepthBiasConstant = shadowSetup.myDepthBiasConstant;
    state.myDepthBiasSlope    = shadowSetup.myDepthBiasSlope;

    CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );

    std::vector< Gfx_ShadowMapPass::DrawItem > draws;
    draws.reserve( aCasterPass.myDraws.size() );
    for ( const Gfx_BatchRun& batch : aCasterPass.myBatchRuns ) {
        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {
            const Gfx_DrawInstance&     draw = aCasterPass.myDraws[ batch.myFirstDrawIndex + drawIndex ];
            const Vk_MeshResource&      mesh = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );
            Gfx_ShadowMapPass::DrawItem item{};
            item.myVertexBuffer  = RhiVulkan::BufferAdopt( mesh.myVertexBuffer.myBuffer );
            item.myIndexBuffer   = RhiVulkan::BufferAdopt( mesh.myIndexBuffer.myBuffer );
            item.myIndexCount    = mesh.myIndexCount;
            item.myDynamicOffset = draw.myInstanceDataOffset;
            draws.push_back( item );
        }
    }

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_ShadowMapPass::GpuResources gpu{};
    gpu.myPipeline    = RhiVulkan::PipelineAdopt( state.myPipeline );
    gpu.myLayout      = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.myObjectSet   = RhiVulkan::DescriptorSetAdopt( aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor );
    gpu.myRenderPass  = RhiVulkan::RenderPassAdopt( state.myRenderPass );
    gpu.myFramebuffer = RhiVulkan::FramebufferAdopt( state.myFramebuffer );

    Gfx_ShadowMapPass::RecordInput input{};
    input.myLightViewProj     = state.myLightViewProj;
    input.myDepthBiasConstant = state.myDepthBiasConstant;
    input.myDepthBiasSlope    = state.myDepthBiasSlope;
    input.myDebugLabels       = aEmitDebugLabels;
    input.myDraws             = draws.data();
    input.myDrawCount         = static_cast< uint32_t >( draws.size() );
    input.myDrawCallsOut      = &aCore.myFrameStats.myDrawCalls;

    Gfx_ShadowMapPass::Record( cmd, gpu, input );

    state.myDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    Rhi::CommandListDestroy( cmd );

    Vk_FrameUniformUploader::UpdateLightingGlobals( aCore, aCore.myFrameCtx.myCurrentFrame, state.myLightViewProj );
}

}  // namespace Vk_ShadowMapPass

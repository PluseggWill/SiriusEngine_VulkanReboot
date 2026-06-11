#include "Vk_ScenePasses.h"

#include "../App/DebugUIState.h"
#include "../Gfx/Gfx_RenderPreset.h"

#include "../Util/Util_Logger.h"

#include "Vk_Bindless.h"

#include "Vk_ClusterBuildPass.h"
#include "Vk_Core.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_GBufferPass.h"

#include "Vk_DescriptorPolicy.h"

#include "../Gfx/Gfx_RenderPacket.h"

#include "Vk_RenderBackend.h"

#include <array>
#include <cstdio>

// POLICY_BINDLESS (Option A): record keyed by Vk_RenderMaterialPath.

// Maint M1/M8: Docs/Archived/plans/shader-bindless-policy_Plan.md §Maintenance contract.

namespace {

constexpr size_t kDrawDebugLabelCapacity = 128;

VkDeviceSize IndirectByteOffset( uint32_t aDrawSlot ) {
    return static_cast< VkDeviceSize >( aDrawSlot ) * sizeof( VkDrawIndexedIndirectCommand );
}

// CPU path: draw-order slot in myDrawIndirectBuffer. GPU cull path: entity SoA slot in myGpuCullIndirectBuffer.
uint32_t ComputeIndirectDrawSlot( bool aUseGpuCullIndirect, uint32_t aViewBaseIndex, uint32_t aPassOffset, uint32_t aDrawIndexInPass, uint32_t aEntitySlot ) {
    return aUseGpuCullIndirect ? Gfx_ComputeEntityIndirectSlot( aViewBaseIndex, aEntitySlot ) : Gfx_ComputeDrawBufferSlot( aViewBaseIndex, aPassOffset, aDrawIndexInPass );
}

// Per-draw RenderDoc tag: fixed stack buffer (no heap); skipped when debug_utils labels unavailable.
void BeginDrawDebugLabel( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const char* aPassName, uint32_t aDrawLabelIndex, const Gfx_DrawInstance& aDraw ) {
    char label[ kDrawDebugLabelCapacity ];
    std::snprintf( label, sizeof( label ), "Pass=%s Draw=%u Mesh=%u Material=%u Entity=%u", aPassName, aDrawLabelIndex, aDraw.myMeshId, aDraw.myMaterialId,
                   aDraw.myEntityIndex );
    aCore.CmdBeginDebugLabel( aCommandBuffer, label );
}

// Per draw: bind VB/IB + Set 2 dynamic offset; issue vkCmdDrawIndexedIndirect (or legacy direct draw).
void RecordSingleIndexedDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, VkPipelineLayout aLayout, VkDescriptorSet aObjectDescriptor,

                              const Gfx_DrawInstance& aDraw, const Gfx_Mesh& aMesh, const char* aPassName, uint32_t aDrawLabelIndex,

                              VkBuffer aIndirectBuffer, VkDeviceSize aIndirectOffset, bool aUseLegacyDirectDraw, bool aEmitDebugLabels ) {

    if ( aEmitDebugLabels ) {
        BeginDrawDebugLabel( aCore, aCommandBuffer, aPassName, aDrawLabelIndex, aDraw );
    }

    VkBuffer vertexBuffers[] = { aMesh.myVertexBuffer.myBuffer };

    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );

    vkCmdBindIndexBuffer( aCommandBuffer, aMesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

    const uint32_t dynamicOffset = aDraw.myInstanceDataOffset;

    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aLayout, VkDescriptorPolicy::kSetObject, 1, &aObjectDescriptor, 1, &dynamicOffset );

    aCore.myFrameStats.myDrawCalls++;

    if ( aUseLegacyDirectDraw ) {

        vkCmdDrawIndexed( aCommandBuffer, aMesh.myIndexCount, 1, 0, 0, 0 );
    }

    else {

        vkCmdDrawIndexedIndirect( aCommandBuffer, aIndirectBuffer, aIndirectOffset, 1, sizeof( VkDrawIndexedIndirectCommand ) );
    }

    if ( aEmitDebugLabels ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

void RecordPassDrawsBatchFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName,

                                     uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw,

                                     bool aEmitDebugLabels, VkPipeline aPipelineOverride ) {

    const VkDescriptorSet objectDescriptor = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor;

    const VkPipelineLayout layout = aCore.mySceneGpuCtx.myPipelineLayout;

    for ( const Gfx_BatchRun& batch : aPass.myBatchRuns ) {

        const Gfx_DrawInstance& firstDraw = aPass.myDraws[ batch.myFirstDrawIndex ];

        const Gfx_Material& material = aCore.mySceneGpuCtx.myResourceTables.GetMaterial( firstDraw.myMaterialId );

        aCore.myFrameStats.myPipelineBinds++;

        // G-buffer path: one MRT pipeline for all batches; forward path uses per-material pipeline.
        const VkPipeline pipeline = aPipelineOverride != VK_NULL_HANDLE ? aPipelineOverride : material.myPipeline;
        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

        const uint32_t materialId = firstDraw.myMaterialId;

        if ( materialId < aCore.mySceneGpuCtx.myMaterialDescriptorSets.size() ) {

            const VkDescriptorSet materialDescriptor = aCore.mySceneGpuCtx.myMaterialDescriptorSets[ materialId ];

            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetMaterial, 1, &materialDescriptor, 0, nullptr );

            aCore.myFrameStats.myMaterialSetBinds++;
        }

        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {

            const uint32_t absoluteDrawIndex = batch.myFirstDrawIndex + drawIndex;

            const Gfx_DrawInstance& draw = aPass.myDraws[ absoluteDrawIndex ];

            const Gfx_Mesh& mesh = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );

            const uint32_t drawSlot =
                ComputeIndirectDrawSlot( aUseGpuCullIndirect, aDrawBufferBaseIndex, aPass.myDrawBufferPassOffset, absoluteDrawIndex, draw.myEntityIndex );
            const VkDeviceSize indirectOffset = IndirectByteOffset( drawSlot );

            RecordSingleIndexedDraw( aCore, aCommandBuffer, layout, objectDescriptor, draw, mesh, aPassName, absoluteDrawIndex, aIndirectBuffer, indirectOffset,

                                     aUseLegacyDirectDraw, aEmitDebugLabels );
        }
    }
}

// Set 1 bound once per view in RecordScene; one graphics pipeline per pass (opaque vs transparent).

void RecordPassDrawsBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline, const char* aPassName,

                                        uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw,

                                        bool aEmitDebugLabels ) {

    const VkDescriptorSet objectDescriptor = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor;

    const VkPipelineLayout layout = aCore.mySceneGpuCtx.myBindlessPipelineLayout;

    aCore.myFrameStats.myPipelineBinds++;

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipeline );

    for ( uint32_t drawIndex = 0; drawIndex < static_cast< uint32_t >( aPass.myDraws.size() ); ++drawIndex ) {

        const Gfx_DrawInstance& draw = aPass.myDraws[ drawIndex ];

        const Gfx_Mesh& mesh = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );

        const uint32_t     drawSlot       = ComputeIndirectDrawSlot( aUseGpuCullIndirect, aDrawBufferBaseIndex, aPass.myDrawBufferPassOffset, drawIndex, draw.myEntityIndex );
        const VkDeviceSize indirectOffset = IndirectByteOffset( drawSlot );

        RecordSingleIndexedDraw( aCore, aCommandBuffer, layout, objectDescriptor, draw, mesh, aPassName, drawIndex, aIndirectBuffer, indirectOffset, aUseLegacyDirectDraw,
                                 aEmitDebugLabels );
    }
}

// M8 (#15): single dispatch keyed by material path; no third record fork.

void RecordPassDrawsFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName,

                                Vk_RenderMaterialPath aMaterialPath, VkPipeline aBindlessPipeline, uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer,

                                bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels, VkPipeline aPipelineOverride ) {

    if ( aPass.myDraws.empty() ) {

        return;
    }

    if ( aMaterialPath == Vk_RenderMaterialPath::Bindless ) {

        RecordPassDrawsBindlessFromPacket( aCore, aCommandBuffer, aPass, aBindlessPipeline, aPassName, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect,
                                           aUseLegacyDirectDraw, aEmitDebugLabels );
    }

    else {

        RecordPassDrawsBatchFromPacket( aCore, aCommandBuffer, aPass, aPassName, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect, aUseLegacyDirectDraw,
                                        aEmitDebugLabels, aPipelineOverride );
    }
}

}  // namespace

// CONTRACT (Stage 1 forward): one swapchain render pass, clear color+depth once.

// Sub-pass A - ForwardOpaque: myOpaquePass, depth write ON, opaque/blend-off pipeline.

// Sub-pass B - ForwardTransparent: myTransparentPass, depth test ON / write OFF, alpha blend pipeline.

// Gfx_FrameRenderPacket order is fixed; Stage 2 FG node ForwardTransparent must read depth from opaque pass.

void Vk_ScenePasses::RecordScene( Vk_Core& aCore, const DebugUIState& aDebugUI, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,

                                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports,

                                  const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,

                                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,

                                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {

    if ( Vk_GBufferPass::IsActive( aCore ) ) {
        if ( !aCore.myGBufferState.myInitialized ) {
            Vk_GBufferPass::Init( aCore );
            Vk_ClusterBuildPass::Init( aCore );
            Vk_DeferredLightingPass::Init( aCore );
        }
        Vk_GBufferPass::RecordFrame( aCore, aDebugUI, aCommandBuffer, anImageIndex, aViewports, aScissors, aFrameDescriptors, aViewCount, aViewPackets );
        return;
    }

    RecordForwardLit( aCore, aDebugUI, aCommandBuffer, anImageIndex, aViewports, aScissors, aFrameDescriptors, aViewCount, aViewPackets );
}

void Vk_ScenePasses::RecordForwardLit( Vk_Core& aCore, const DebugUIState& aDebugUI, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,

                                       const std::array< VkViewport, kGfxMaxRenderViews >& aViewports,

                                       const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,

                                       const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,

                                       const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {

    VkRenderPassBeginInfo renderPassInfo{};

    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    renderPassInfo.renderPass = aCore.mySwapchainCtx.myRenderPass;

    renderPassInfo.framebuffer = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ anImageIndex ];

    renderPassInfo.renderArea.offset = { 0, 0 };

    renderPassInfo.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;

    std::array< VkClearValue, 2 > clearValues{};

    clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    clearValues[ 1 ].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast< uint32_t >( clearValues.size() );

    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    static bool sPacketPathLoggedOnce = false;

    static bool sPacketSkipLoggedOnce = false;

    static bool sIndirectPathLoggedOnce = false;

    static bool sGpuIndirectPathLoggedOnce = false;

    const Vk_RenderMaterialPath materialPath = aCore.myDeviceCtx.myMaterialPath;

    const bool bindless = materialPath == Vk_RenderMaterialPath::Bindless;

    const bool legacyDirectDraw = aCore.EngineConfig().GetLegacyDirectDraw();
    const bool gpuCullRecord    = aCore.EngineConfig().GetGpuCullEnabled() && !legacyDirectDraw;
    const bool emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();

    const VkPipelineLayout frameBindLayout = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;

    Vk_FrameData&  frame          = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
    const VkBuffer indirectBuffer = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;

    for ( uint32_t viewIndex = 0; viewIndex < aViewCount; ++viewIndex ) {

        const Gfx_FrameRenderPacket& packet = aViewPackets[ viewIndex ];

        aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex ], aScissors[ viewIndex ] );

        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameBindLayout, VkDescriptorPolicy::kSetFrame, 1, &aFrameDescriptors[ viewIndex ], 0,

                                 nullptr );

        if ( !Vk_RenderBackend::ValidateFramePacket( packet ) ) {

            if ( !sPacketSkipLoggedOnce ) {

                UtilLogger::Warn( "RENDER", "Packet invalid; scene draw record skipped." );

                sPacketSkipLoggedOnce = true;
            }

            continue;
        }

        if ( bindless ) {

            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,

                                     &aCore.mySceneGpuCtx.myBindlessDescriptorSet, 0, nullptr );

            aCore.myFrameStats.myMaterialSetBinds++;
        }

        if ( !aDebugUI.myRenderDebug.mySkipOpaquePass ) {

            if ( emitDebugLabels ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Opaque" );
            }

            RecordPassDrawsFromPacket( aCore, aCommandBuffer, packet.myOpaquePass, "Opaque", materialPath, aCore.mySceneGpuCtx.myBasicPipelineBindless,

                                       packet.myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord, legacyDirectDraw, emitDebugLabels, VK_NULL_HANDLE );

            if ( emitDebugLabels ) {
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }

        if ( !aDebugUI.myRenderDebug.mySkipTransparentPass ) {

            if ( emitDebugLabels ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Transparent" );
            }

            RecordPassDrawsFromPacket( aCore, aCommandBuffer, packet.myTransparentPass, "Transparent", materialPath,

                                       aCore.mySceneGpuCtx.myTransparentPipelineBindless, packet.myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord, legacyDirectDraw,
                                       emitDebugLabels, VK_NULL_HANDLE );

            if ( emitDebugLabels ) {
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }
    }

    if ( aViewCount > 0 && !sPacketPathLoggedOnce ) {

        UtilLogger::Info( "RENDER", "Scene record switched to packet consume path." );

        sPacketPathLoggedOnce = true;
    }

    if ( aViewCount > 0 && gpuCullRecord && !sGpuIndirectPathLoggedOnce ) {

        UtilLogger::Info( "RENDER", "Scene record using GPU-filled slot indirect (EntityCull.comp → myGpuCullIndirectBuffer)." );

        sGpuIndirectPathLoggedOnce = true;
    }

    if ( aViewCount > 0 && !legacyDirectDraw && !gpuCullRecord && !sIndirectPathLoggedOnce ) {

        UtilLogger::Info( "RENDER", "Scene record using CPU vkCmdDrawIndexedIndirect (draw templates uploaded each frame)." );

        sIndirectPathLoggedOnce = true;
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

void Vk_ScenePasses::RecordOpaquePacketDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, uint32_t aDrawBufferBaseIndex,
                                              VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels,
                                              VkPipeline aGBufferPipeline ) {

    const Vk_RenderMaterialPath path = aCore.myDeviceCtx.myMaterialPath;
    if ( path == Vk_RenderMaterialPath::Bindless ) {
        RecordPassDrawsFromPacket( aCore, aCommandBuffer, aPass, "GBufferOpaque", path, aGBufferPipeline, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect,
                                   aUseLegacyDirectDraw, aEmitDebugLabels, VK_NULL_HANDLE );
    }
    else {
        RecordPassDrawsFromPacket( aCore, aCommandBuffer, aPass, "GBufferOpaque", path, VK_NULL_HANDLE, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect,
                                   aUseLegacyDirectDraw, aEmitDebugLabels, aGBufferPipeline );
    }
}

void Vk_ScenePasses::RecordTransparentPacketDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, uint32_t aDrawBufferBaseIndex,
                                                   VkBuffer aIndirectBuffer, bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels ) {

    const Vk_RenderMaterialPath path = aCore.myDeviceCtx.myMaterialPath;
    const bool                  hybridResolve =
        Gfx_RenderPreset::IsHybridDeferred( aCore.EngineConfig().GetRenderPresetName() ) && aCore.mySwapchainCtx.myHybridResolveRenderPass != VK_NULL_HANDLE;
    const VkPipeline bindlessPipeline = path == Vk_RenderMaterialPath::Bindless ? ( hybridResolve ? aCore.mySceneGpuCtx.myTransparentPipelineBindlessHybridResolve
                                                                                                  : aCore.mySceneGpuCtx.myTransparentPipelineBindless )
                                                                                : VK_NULL_HANDLE;
    const VkPipeline batchPipelineOverride =
        path != Vk_RenderMaterialPath::Bindless && hybridResolve ? aCore.mySceneGpuCtx.myTransparentPipelineHybridResolve : VK_NULL_HANDLE;
    RecordPassDrawsFromPacket( aCore, aCommandBuffer, aPass, "ForwardTransparent", path, bindlessPipeline, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect,
                               aUseLegacyDirectDraw, aEmitDebugLabels, batchPipelineOverride );
}

void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {

    aCore.myPlatformCtx.myImGuiLayer.Render( aCommandBuffer, anImageIndex, aCore.mySwapchainCtx.mySwapChainExtent );
}

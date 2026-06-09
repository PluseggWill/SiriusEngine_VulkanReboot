#include "Vk_ScenePasses.h"



#include "../App/DebugUIState.h"

#include "../Util/Util_Logger.h"

#include "Vk_Bindless.h"

#include "Vk_Core.h"

#include "Vk_DescriptorPolicy.h"

#include "Vk_RenderBackend.h"



#include <array>

#include <string>



// POLICY_BINDLESS (Option A): record keyed by Vk_RenderMaterialPath.

// Maint M1/M8: Docs/Archived/plans/shader-bindless-policy_Plan.md §Maintenance contract.



namespace {



void RecordSingleIndexedDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, VkPipelineLayout aLayout, VkDescriptorSet aObjectDescriptor,

                              const Gfx_DrawInstance& aDraw, const Gfx_Mesh& aMesh, const char* aPassName, uint32_t aDrawLabelIndex ) {

    const std::string drawTag = std::string( "Pass=" ) + aPassName + " Draw=" + std::to_string( aDrawLabelIndex ) + " Mesh=" + std::to_string( aDraw.myMeshId )

                                + " Material=" + std::to_string( aDraw.myMaterialId ) + " Entity=" + std::to_string( aDraw.myEntityIndex );

    aCore.CmdBeginDebugLabel( aCommandBuffer, drawTag.c_str() );



    VkBuffer     vertexBuffers[] = { aMesh.myVertexBuffer.myBuffer };

    VkDeviceSize offsets[]       = { 0 };

    vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );

    vkCmdBindIndexBuffer( aCommandBuffer, aMesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );



    const uint32_t dynamicOffset = aDraw.myInstanceDataOffset;

    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aLayout, VkDescriptorPolicy::kSetObject, 1, &aObjectDescriptor, 1, &dynamicOffset );

    aCore.myFrameStats.myDrawCalls++;

    vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( aMesh.myIndices.size() ), 1, 0, 0, 0 );

    aCore.CmdEndDebugLabel( aCommandBuffer );

}



void RecordPassDrawsBatchFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName ) {

    const VkDescriptorSet  objectDescriptor = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor;

    const VkPipelineLayout layout           = aCore.mySceneGpuCtx.myPipelineLayout;



    for ( const Gfx_BatchRun& batch : aPass.myBatchRuns ) {

        const Gfx_DrawInstance& firstDraw = aPass.myDraws[ batch.myFirstDrawIndex ];

        const Gfx_Material&     material  = aCore.mySceneGpuCtx.myResourceTables.GetMaterial( firstDraw.myMaterialId );



        aCore.myFrameStats.myPipelineBinds++;

        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.myPipeline );



        const uint32_t materialId = firstDraw.myMaterialId;

        if ( materialId < aCore.mySceneGpuCtx.myMaterialDescriptorSets.size() ) {

            const VkDescriptorSet materialDescriptor = aCore.mySceneGpuCtx.myMaterialDescriptorSets[ materialId ];

            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetMaterial, 1, &materialDescriptor, 0, nullptr );

            aCore.myFrameStats.myMaterialSetBinds++;

        }



        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {

            const uint32_t          absoluteDrawIndex = batch.myFirstDrawIndex + drawIndex;

            const Gfx_DrawInstance& draw              = aPass.myDraws[ absoluteDrawIndex ];

            const Gfx_Mesh&         mesh              = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );

            RecordSingleIndexedDraw( aCore, aCommandBuffer, layout, objectDescriptor, draw, mesh, aPassName, absoluteDrawIndex );

        }

    }

}



// Set 1 bound once per view in RecordScene; one graphics pipeline per pass (opaque vs transparent).

void RecordPassDrawsBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline, const char* aPassName ) {

    const VkDescriptorSet  objectDescriptor = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor;

    const VkPipelineLayout layout           = aCore.mySceneGpuCtx.myBindlessPipelineLayout;



    aCore.myFrameStats.myPipelineBinds++;

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipeline );



    for ( uint32_t drawIndex = 0; drawIndex < static_cast< uint32_t >( aPass.myDraws.size() ); ++drawIndex ) {

        const Gfx_DrawInstance& draw = aPass.myDraws[ drawIndex ];

        const Gfx_Mesh&         mesh = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );

        RecordSingleIndexedDraw( aCore, aCommandBuffer, layout, objectDescriptor, draw, mesh, aPassName, drawIndex );

    }

}



// M8 (#15): single dispatch keyed by material path; no third record fork.

void RecordPassDrawsFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName,

                                Vk_RenderMaterialPath aMaterialPath, VkPipeline aBindlessPipeline ) {

    if ( aPass.myDraws.empty() ) {

        return;

    }



    if ( aMaterialPath == Vk_RenderMaterialPath::Bindless ) {

        RecordPassDrawsBindlessFromPacket( aCore, aCommandBuffer, aPass, aBindlessPipeline, aPassName );

    }

    else {

        RecordPassDrawsBatchFromPacket( aCore, aCommandBuffer, aPass, aPassName );

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

    VkRenderPassBeginInfo renderPassInfo{};

    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    renderPassInfo.renderPass        = aCore.mySwapchainCtx.myRenderPass;

    renderPassInfo.framebuffer       = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ anImageIndex ];

    renderPassInfo.renderArea.offset = { 0, 0 };

    renderPassInfo.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;



    std::array< VkClearValue, 2 > clearValues{};

    clearValues[ 0 ].color         = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    clearValues[ 1 ].depthStencil  = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast< uint32_t >( clearValues.size() );

    renderPassInfo.pClearValues    = clearValues.data();



    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );



    static bool sPacketPathLoggedOnce = false;

    static bool sPacketSkipLoggedOnce = false;



    const Vk_RenderMaterialPath materialPath    = aCore.myDeviceCtx.myMaterialPath;

    const bool                  bindless        = materialPath == Vk_RenderMaterialPath::Bindless;

    const VkPipelineLayout      frameBindLayout = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;

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

            aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Opaque" );

            RecordPassDrawsFromPacket( aCore, aCommandBuffer, packet.myOpaquePass, "Opaque", materialPath, aCore.mySceneGpuCtx.myBasicPipelineBindless );

            aCore.CmdEndDebugLabel( aCommandBuffer );

        }

        if ( !aDebugUI.myRenderDebug.mySkipTransparentPass ) {

            aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Transparent" );

            RecordPassDrawsFromPacket( aCore, aCommandBuffer, packet.myTransparentPass, "Transparent", materialPath,

                                       aCore.mySceneGpuCtx.myTransparentPipelineBindless );

            aCore.CmdEndDebugLabel( aCommandBuffer );

        }

    }



    if ( aViewCount > 0 && !sPacketPathLoggedOnce ) {

        UtilLogger::Info( "RENDER", "Scene record switched to packet consume path." );

        sPacketPathLoggedOnce = true;

    }



    vkCmdEndRenderPass( aCommandBuffer );

}



void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {

    aCore.myPlatformCtx.myImGuiLayer.Render( aCommandBuffer, anImageIndex, aCore.mySwapchainCtx.mySwapChainExtent );

}



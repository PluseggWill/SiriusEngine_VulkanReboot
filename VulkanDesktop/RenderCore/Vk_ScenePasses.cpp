#include "Vk_ScenePasses.h"

#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_RenderBackend.h"

#include <array>
#include <string>

void Vk_ScenePasses::RecordDrawBatchesFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, const char* aPassName ) {
    const VkDescriptorSet  objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout           = aCore.myPipelineLayout;

    for ( const Gfx_BatchRun& batch : aPass.myBatchRuns ) {
        const Gfx_DrawInstance& firstDraw = aPass.myDraws[ batch.myFirstDrawIndex ];
        const Gfx_Material&     material  = aCore.myResourceTables.GetMaterial( firstDraw.myMaterialId );

        aCore.myFrameStats.myPipelineBinds++;
        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.myPipeline );

        const uint32_t materialId = firstDraw.myMaterialId;
        if ( materialId < aCore.myMaterialDescriptorSets.size() ) {
            const VkDescriptorSet materialDescriptor = aCore.myMaterialDescriptorSets[ materialId ];
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetMaterial, 1, &materialDescriptor, 0, nullptr );
            aCore.myFrameStats.myMaterialSetBinds++;
        }

        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {
            const uint32_t          absoluteDrawIndex = batch.myFirstDrawIndex + drawIndex;
            const Gfx_DrawInstance& draw              = aPass.myDraws[ absoluteDrawIndex ];
            const Gfx_Mesh&         mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );
            const std::string drawTag   = std::string( "Pass=" ) + aPassName + " Draw=" + std::to_string( absoluteDrawIndex ) + " Mesh=" + std::to_string( draw.myMeshId )
                                        + " Material=" + std::to_string( draw.myMaterialId ) + " Entity=" + std::to_string( draw.myEntityIndex );
            aCore.CmdBeginDebugLabel( aCommandBuffer, drawTag.c_str() );

            VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
            VkDeviceSize offsets[]       = { 0 };
            vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
            vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

            const uint32_t dynamicOffset = draw.myInstanceDataOffset;
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );
            aCore.myFrameStats.myDrawCalls++;
            vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( mesh.myIndices.size() ), 1, 0, 0, 0 );
            aCore.CmdEndDebugLabel( aCommandBuffer );
        }
    }
}

void Vk_ScenePasses::RecordDrawBatchesBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline,
                                                          const char* aPassName ) {
    const VkDescriptorSet  objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout           = aCore.myBindlessPipelineLayout;

    if ( aPass.myDraws.empty() ) {
        return;
    }

    aCore.myFrameStats.myPipelineBinds++;
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipeline );

    for ( uint32_t drawIndex = 0; drawIndex < static_cast< uint32_t >( aPass.myDraws.size() ); ++drawIndex ) {
        const Gfx_DrawInstance& draw = aPass.myDraws[ drawIndex ];
        const Gfx_Mesh& mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );
        const std::string drawTag =
            std::string( "Pass=" ) + aPassName + " Draw=" + std::to_string( drawIndex ) + " Mesh=" + std::to_string( draw.myMeshId ) + " Material="
            + std::to_string( draw.myMaterialId ) + " Entity=" + std::to_string( draw.myEntityIndex );
        aCore.CmdBeginDebugLabel( aCommandBuffer, drawTag.c_str() );

        VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
        VkDeviceSize offsets[]       = { 0 };
        vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
        vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

        const uint32_t dynamicOffset = draw.myInstanceDataOffset;
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );
        aCore.myFrameStats.myDrawCalls++;
        vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( mesh.myIndices.size() ), 1, 0, 0, 0 );
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

// CONTRACT (Stage 1 forward): one swapchain render pass, clear color+depth once.
// Sub-pass A — ForwardOpaque: myOpaquePass, depth write ON, opaque/blend-off pipeline.
// Sub-pass B — ForwardTransparent: myTransparentPass, depth test ON / write OFF, alpha blend pipeline.
// Gfx_FrameRenderPacket order is fixed; Stage 2 FG node ForwardTransparent must read depth from opaque pass.
void Vk_ScenePasses::RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex, const std::array< VkViewport, kGfxMaxRenderViews >& aViewports,
                                  const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = aCore.myRenderPass;
    renderPassInfo.framebuffer       = aCore.mySwapChainFrameBuffers[ anImageIndex ];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = aCore.mySwapChainExtent;

    std::array< VkClearValue, 2 > clearValues{};
    clearValues[ 0 ].color         = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[ 1 ].depthStencil  = { 1.0f, 0 };
    renderPassInfo.clearValueCount = static_cast< uint32_t >( clearValues.size() );
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    static bool sPacketPathLoggedOnce = false;
    static bool sPacketSkipLoggedOnce = false;

    const VkPipelineLayout frameBindLayout = aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ? aCore.myBindlessPipelineLayout : aCore.myPipelineLayout;
    for ( uint32_t viewIndex = 0; viewIndex < aViewCount; ++viewIndex ) {
        const Gfx_FrameRenderPacket& packet = aViewPackets[ viewIndex ];
        aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex ], aScissors[ viewIndex ] );
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, frameBindLayout, VkDescriptorPolicy::kSetFrame, 1, &aFrameDescriptors[ viewIndex ], 0,
                                 nullptr );

        const bool usePacketPath = Vk_RenderBackend::ValidateFramePacket( packet );
        if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                     &aCore.myBindlessDescriptorSet, 0, nullptr );
            aCore.myFrameStats.myMaterialSetBinds++;
            if ( usePacketPath ) {
                if ( !aCore.myRenderDebugState.mySkipOpaquePass ) {
                    aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Opaque" );
                    RecordDrawBatchesBindlessFromPacket( aCore, aCommandBuffer, packet.myOpaquePass, aCore.myBasicPipelineBindless, "Opaque" );
                    aCore.CmdEndDebugLabel( aCommandBuffer );
                }
                if ( !aCore.myRenderDebugState.mySkipTransparentPass ) {
                    aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Transparent" );
                    RecordDrawBatchesBindlessFromPacket( aCore, aCommandBuffer, packet.myTransparentPass, aCore.myTransparentPipelineBindless, "Transparent" );
                    aCore.CmdEndDebugLabel( aCommandBuffer );
                }
            }
            else if ( !sPacketSkipLoggedOnce ) {
                UtilLogger::Warn( "RENDER", "Packet invalid; scene draw record skipped." );
                sPacketSkipLoggedOnce = true;
            }
        }
        else if ( usePacketPath ) {
            if ( !aCore.myRenderDebugState.mySkipOpaquePass ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Opaque" );
                RecordDrawBatchesFromPacket( aCore, aCommandBuffer, packet.myOpaquePass, "Opaque" );
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
            if ( !aCore.myRenderDebugState.mySkipTransparentPass ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Transparent" );
                RecordDrawBatchesFromPacket( aCore, aCommandBuffer, packet.myTransparentPass, "Transparent" );
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }
        else if ( !sPacketSkipLoggedOnce ) {
            UtilLogger::Warn( "RENDER", "Packet invalid; scene draw record skipped." );
            sPacketSkipLoggedOnce = true;
        }
    }

    if ( aViewCount > 0 && !sPacketPathLoggedOnce ) {
        UtilLogger::Info( "RENDER", "Scene record switched to packet consume path." );
        sPacketPathLoggedOnce = true;
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    aCore.myImGuiLayer.Render( aCommandBuffer, anImageIndex, aCore.mySwapChainExtent );
}

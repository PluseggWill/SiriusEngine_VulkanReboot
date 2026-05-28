#include "Vk_ScenePasses.h"

#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_RenderBackend.h"
#include "../Util/Util_Logger.h"

#include <array>

void Vk_ScenePasses::RecordDrawBatchesFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass ) {
    const VkDescriptorSet objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout          = aCore.myPipelineLayout;

    for ( const Gfx_BatchRun& batch : aPass.myBatchRuns ) {
        const Gfx_DrawInstance& firstDraw = aPass.myDraws[ batch.myFirstDrawIndex ];
        const Gfx_Material&     material  = aCore.myResourceTables.GetMaterial( firstDraw.myMaterialId );

        aCore.myFrameStats.myPipelineBinds++;
        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.myPipeline );
        aCore.SetGraphicsDynamicState( aCommandBuffer );

        const uint32_t materialId = firstDraw.myMaterialId;
        if ( materialId < aCore.myMaterialDescriptorSets.size() ) {
            const VkDescriptorSet materialDescriptor = aCore.myMaterialDescriptorSets[ materialId ];
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetMaterial, 1, &materialDescriptor, 0, nullptr );
            aCore.myFrameStats.myMaterialSetBinds++;
        }

        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {
            const Gfx_DrawInstance& draw = aPass.myDraws[ batch.myFirstDrawIndex + drawIndex ];
            const Gfx_Mesh&         mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );

            VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
            VkDeviceSize offsets[]       = { 0 };
            vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
            vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

            // One draw = one dynamic offset into the frame-local instance slab (Set 2).
            const uint32_t dynamicOffset = draw.myInstanceDataOffset;
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );
            aCore.myFrameStats.myDrawCalls++;
            vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( mesh.myIndices.size() ), 1, 0, 0, 0 );
        }
    }
}

void Vk_ScenePasses::RecordDrawBatchesBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline ) {
    const VkDescriptorSet objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout          = aCore.myBindlessPipelineLayout;

    if ( aPass.myDraws.empty() ) {
        return;
    }

    aCore.myFrameStats.myPipelineBinds++;
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipeline );
    aCore.SetGraphicsDynamicState( aCommandBuffer );

    for ( const Gfx_DrawInstance& draw : aPass.myDraws ) {
        const Gfx_Mesh& mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );

        VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
        VkDeviceSize offsets[]       = { 0 };
        vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
        vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

        // Bind per-draw transform/material index slice via dynamic UBO offset.
        const uint32_t dynamicOffset = draw.myInstanceDataOffset;
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );
        aCore.myFrameStats.myDrawCalls++;
        vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( mesh.myIndices.size() ), 1, 0, 0, 0 );
    }
}

void Vk_ScenePasses::RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = aCore.myRenderPass;
    renderPassInfo.framebuffer       = aCore.mySwapChainFrameBuffers[ anImageIndex ];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = aCore.mySwapChainExtent;

    std::array< VkClearValue, 2 > clearValues{};
    clearValues[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[ 1 ].depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = static_cast< uint32_t >( clearValues.size() );
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    const VkDescriptorSet frameDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myGlobalDescriptor;
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myPipelineLayout, VkDescriptorPolicy::kSetFrame, 1, &frameDescriptor, 0, nullptr );

    static bool sPacketPathLoggedOnce   = false;
    static bool sPacketSkipLoggedOnce   = false;
    const bool  usePacketPath         = Vk_RenderBackend::ValidateFramePacket( aCore.myDrawPrep.myFramePacket );

    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                 &aCore.myBindlessDescriptorSet, 0, nullptr );
        aCore.myFrameStats.myMaterialSetBinds++;
        if ( usePacketPath ) {
            RecordDrawBatchesBindlessFromPacket( aCore, aCommandBuffer, aCore.myDrawPrep.myFramePacket.myOpaquePass, aCore.myBasicPipelineBindless );
            RecordDrawBatchesBindlessFromPacket( aCore, aCommandBuffer, aCore.myDrawPrep.myFramePacket.myTransparentPass, aCore.myTransparentPipelineBindless );
        }
        else {
            if ( !sPacketSkipLoggedOnce ) {
                UtilLogger::Warn( "RENDER", "Packet invalid; scene draw record skipped." );
                sPacketSkipLoggedOnce = true;
            }
        }
    }
    else {
        if ( usePacketPath ) {
            RecordDrawBatchesFromPacket( aCore, aCommandBuffer, aCore.myDrawPrep.myFramePacket.myOpaquePass );
            RecordDrawBatchesFromPacket( aCore, aCommandBuffer, aCore.myDrawPrep.myFramePacket.myTransparentPass );
        }
        else {
            if ( !sPacketSkipLoggedOnce ) {
                UtilLogger::Warn( "RENDER", "Packet invalid; scene draw record skipped." );
                sPacketSkipLoggedOnce = true;
            }
        }
    }

    if ( usePacketPath && !sPacketPathLoggedOnce ) {
        UtilLogger::Info( "RENDER", "Scene record switched to packet consume path." );
        sPacketPathLoggedOnce = true;
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    aCore.myImGuiLayer.Render( aCommandBuffer, anImageIndex, aCore.mySwapChainExtent );
}

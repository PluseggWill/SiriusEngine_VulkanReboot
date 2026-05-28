#include "Vk_ScenePasses.h"

#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"

#include <array>

void Vk_ScenePasses::RecordDrawBatches( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, const std::vector< Gfx_BatchRun >& aBatchRuns ) {
    const VkDescriptorSet objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout          = aCore.myPipelineLayout;

    for ( const Gfx_BatchRun& batch : aBatchRuns ) {
        const Gfx_DrawInstance& firstDraw = aExtract.myDrawInstances[ batch.myFirstDrawIndex ];
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
            const Gfx_DrawInstance& draw = aExtract.myDrawInstances[ batch.myFirstDrawIndex + drawIndex ];
            const Gfx_Mesh&         mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );

            VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
            VkDeviceSize offsets[]       = { 0 };
            vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
            vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

            const uint32_t dynamicOffset = draw.myInstanceDataOffset;
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );
            aCore.myFrameStats.myDrawCalls++;
            vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( mesh.myIndices.size() ), 1, 0, 0, 0 );
        }
    }
}

void Vk_ScenePasses::RecordDrawBatchesBindless( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, VkPipeline aPipeline ) {
    const VkDescriptorSet objectDescriptor = aCore.myFrameDatas[ aCore.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout          = aCore.myBindlessPipelineLayout;

    if ( aExtract.myDrawInstances.empty() ) {
        return;
    }

    aCore.myFrameStats.myPipelineBinds++;
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipeline );
    aCore.SetGraphicsDynamicState( aCommandBuffer );

    for ( const Gfx_DrawInstance& draw : aExtract.myDrawInstances ) {
        const Gfx_Mesh& mesh = aCore.myResourceTables.GetMesh( draw.myMeshId );

        VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
        VkDeviceSize offsets[]       = { 0 };
        vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
        vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

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

    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                 &aCore.myBindlessDescriptorSet, 0, nullptr );
        aCore.myFrameStats.myMaterialSetBinds++;
        RecordDrawBatchesBindless( aCore, aCommandBuffer, aCore.myDrawPrep.myExtract.myOpaque, aCore.myBasicPipelineBindless );
        RecordDrawBatchesBindless( aCore, aCommandBuffer, aCore.myDrawPrep.myExtract.myTransparent, aCore.myTransparentPipelineBindless );
    }
    else {
        RecordDrawBatches( aCore, aCommandBuffer, aCore.myDrawPrep.myExtract.myOpaque, aCore.myDrawPrep.myOpaqueBatchRuns );
        RecordDrawBatches( aCore, aCommandBuffer, aCore.myDrawPrep.myExtract.myTransparent, aCore.myDrawPrep.myTransparentBatchRuns );
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    aCore.myImGuiLayer.Render( aCommandBuffer, anImageIndex, aCore.mySwapChainExtent );
}

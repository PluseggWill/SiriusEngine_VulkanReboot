#include "Gfx_ShadowMapPass.h"

namespace Gfx_ShadowMapPass {

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    Rhi::ClearValue clear{};
    clear.myType    = Rhi_ClearValueType::DepthStencil;
    clear.myDepth   = 0.0f;
    clear.myStencil = 0;

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = aGpu.myRenderPass;
    begin.myFramebuffer = aGpu.myFramebuffer;
    begin.myWidth       = kMapSize;
    begin.myHeight      = kMapSize;
    begin.myClears      = &clear;
    begin.myClearCount  = 1;

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=ShadowMap" );
    }

    Rhi::CommandListBeginRenderPass( aCmd, begin );
    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myPipeline );
    Rhi::CommandListSetViewport( aCmd, Rhi::Viewport{ 0.f, 0.f, static_cast< float >( kMapSize ), static_cast< float >( kMapSize ), 0.f, 1.f } );
    Rhi::CommandListSetScissor( aCmd, Rhi::Scissor{ 0, 0, kMapSize, kMapSize } );
    Rhi::CommandListSetDepthBias( aCmd, aInput.myDepthBiasConstant, 0.0f, aInput.myDepthBiasSlope );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Vertex, 0, sizeof( aInput.myLightViewProj ), &aInput.myLightViewProj );

    for ( uint32_t i = 0; i < aInput.myDrawCount; ++i ) {
        const DrawItem& draw = aInput.myDraws[ i ];
        Rhi::CommandListBindVertexBuffer( aCmd, 0, draw.myVertexBuffer, 0 );
        Rhi::CommandListBindIndexBuffer( aCmd, draw.myIndexBuffer, 0, Rhi_IndexType::Uint32 );
        Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myLayout, 0, aGpu.myObjectSet, &draw.myDynamicOffset, 1 );
        if ( aInput.myDrawCallsOut != nullptr ) {
            ( *aInput.myDrawCallsOut )++;
        }
        Rhi::CommandListDrawIndexed( aCmd, draw.myIndexCount, 1, 0, 0, 0 );
    }

    Rhi::CommandListEndRenderPass( aCmd );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

}  // namespace Gfx_ShadowMapPass

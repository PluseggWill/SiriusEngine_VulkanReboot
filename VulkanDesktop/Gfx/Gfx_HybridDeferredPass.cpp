#include "Gfx_HybridDeferredPass.h"

#include <array>

namespace Gfx_HybridDeferredPass {

bool BeginGBuffer( Rhi_CommandList& aCmd, const GBufferGpu& aGpu ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) || !aGpu.myRenderPass || !aGpu.myFramebuffer || aGpu.myWidth == 0 || aGpu.myHeight == 0 ) {
        return false;
    }

    // Match Vk_GBufferPass attachment order: albedo, normalRoughness, worldPos, motionVector, depth.
    const std::array< Rhi::ClearValue, 5 > clears = {
        Rhi::MakeClearColor( 0.0f, 0.0f, 0.0f, 1.0f ), Rhi::MakeClearColor( 0.0f, 0.0f, 1.0f, 0.5f ), Rhi::MakeClearColor( 0.0f, 0.0f, 0.0f, 0.0f ),
        Rhi::MakeClearColor( 0.0f, 0.0f, 0.0f, 0.0f ), Rhi::MakeClearDepthStencil( 1.0f, 0 ),
    };

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = aGpu.myRenderPass;
    begin.myFramebuffer = aGpu.myFramebuffer;
    begin.myWidth       = aGpu.myWidth;
    begin.myHeight      = aGpu.myHeight;
    begin.myClears      = clears.data();
    begin.myClearCount  = static_cast< uint32_t >( clears.size() );
    Rhi::CommandListBeginRenderPass( aCmd, begin );
    return true;
}

void EndGBuffer( Rhi_CommandList& aCmd ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }
    Rhi::CommandListEndRenderPass( aCmd );
}

bool BeginHybrid( Rhi_CommandList& aCmd, const HybridGpu& aGpu ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) || !aGpu.myRenderPass || !aGpu.myFramebuffer || aGpu.myWidth == 0 || aGpu.myHeight == 0 ) {
        return false;
    }

    // Color LOAD_OP_CLEAR; depth attachment is LOAD_OP_LOAD (copied GBuffer depth) — clear unused but count must match RP.
    const std::array< Rhi::ClearValue, 2 > clears = {
        Rhi::MakeClearColor( 0.0f, 0.0f, 0.0f, 1.0f ),
        Rhi::MakeClearDepthStencil( 1.0f, 0 ),
    };

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = aGpu.myRenderPass;
    begin.myFramebuffer = aGpu.myFramebuffer;
    begin.myWidth       = aGpu.myWidth;
    begin.myHeight      = aGpu.myHeight;
    begin.myClears      = clears.data();
    begin.myClearCount  = static_cast< uint32_t >( clears.size() );
    Rhi::CommandListBeginRenderPass( aCmd, begin );
    return true;
}

void EndHybrid( Rhi_CommandList& aCmd ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }
    Rhi::CommandListEndRenderPass( aCmd );
}

}  // namespace Gfx_HybridDeferredPass

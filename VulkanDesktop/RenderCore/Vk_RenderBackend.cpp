#include "Vk_RenderBackend.h"

bool Vk_RenderBackend::ValidateFramePacket( const Gfx_FrameRenderPacket& aPacket ) {
    // Visible draws should never exceed pre-cull candidate count from the same frame build.
    const size_t drawCount = aPacket.myOpaquePass.myDraws.size() + aPacket.myTransparentPass.myDraws.size();
    return drawCount <= aPacket.myDrawCountBeforeCull;
}

bool Vk_RenderBackend::ValidateFramePacketParity( const Gfx_FrameRenderPacket& aPacket, size_t aOpaqueDrawCount, size_t aTransparentDrawCount, size_t aOpaqueBatchCount,
                                                  size_t aTransparentBatchCount ) {
    return aPacket.myOpaquePass.myDraws.size() == aOpaqueDrawCount && aPacket.myTransparentPass.myDraws.size() == aTransparentDrawCount
           && aPacket.myOpaquePass.myBatchRuns.size() == aOpaqueBatchCount && aPacket.myTransparentPass.myBatchRuns.size() == aTransparentBatchCount;
}

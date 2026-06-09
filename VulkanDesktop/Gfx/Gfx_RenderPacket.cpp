#include "Gfx_RenderPacket.h"

void Gfx_BuildFrameRenderPacketFromStream( const Gfx_FrameDrawStreamOutput& aStreamOutput, Gfx_FrameRenderPacket& aOutPacket ) {
    // Packet is a plain transfer object: copy stream results without mutating render semantics.
    aOutPacket.myDrawCountBeforeCull                    = aStreamOutput.myDrawCountBeforeCull;
    aOutPacket.myOpaquePass.myDraws                     = aStreamOutput.myExtract.myOpaque.myDrawInstances;
    aOutPacket.myTransparentPass.myDraws                = aStreamOutput.myExtract.myTransparent.myDrawInstances;
    aOutPacket.myOpaquePass.myBatchRuns                 = aStreamOutput.myOpaqueBatchRuns;
    aOutPacket.myTransparentPass.myBatchRuns            = aStreamOutput.myTransparentBatchRuns;
    aOutPacket.myOpaquePass.myDrawBufferPassOffset      = 0;
    aOutPacket.myTransparentPass.myDrawBufferPassOffset = static_cast< uint32_t >( aOutPacket.myOpaquePass.myDraws.size() );
}

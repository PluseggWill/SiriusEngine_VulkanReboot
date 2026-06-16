#include "Gfx_FramePacketValidation.h"

namespace Gfx_FramePacketValidation {

bool ValidateFramePacket( const Gfx_FrameRenderPacket& aPacket ) {
    const size_t drawCount = aPacket.myOpaquePass.myDraws.size() + aPacket.myTransparentPass.myDraws.size();
    return drawCount <= aPacket.myDrawCountBeforeCull;
}

}  // namespace Gfx_FramePacketValidation

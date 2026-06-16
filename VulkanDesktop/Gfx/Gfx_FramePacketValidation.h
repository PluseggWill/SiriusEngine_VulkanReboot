#pragma once

#include "Gfx_RenderPacket.h"

namespace Gfx_FramePacketValidation {

// Visible draws should never exceed pre-cull candidates from the same frame build.
bool ValidateFramePacket( const Gfx_FrameRenderPacket& aPacket );

}  // namespace Gfx_FramePacketValidation

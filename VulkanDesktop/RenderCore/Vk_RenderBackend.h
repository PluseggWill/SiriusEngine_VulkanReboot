#pragma once

#include "../Gfx/Gfx_RenderPacket.h"

// Render backend boundary helpers for packet-consume migration.
// P1 scope: contract validation only (no execution-path switch yet).
class Vk_RenderBackend {
public:
    // Structural sanity check used at consume boundary before issuing draw commands.
    static bool ValidateFramePacket( const Gfx_FrameRenderPacket& aPacket );
    // Legacy parity helper retained for diagnostics/history; can be removed after closeout freeze.
    static bool ValidateFramePacketParity( const Gfx_FrameRenderPacket& aPacket, size_t aOpaqueDrawCount, size_t aTransparentDrawCount, size_t aOpaqueBatchCount,
                                           size_t aTransparentBatchCount );
};

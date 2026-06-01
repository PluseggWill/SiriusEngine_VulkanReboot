#pragma once

#include <cstddef>
#include <vector>

#include "Gfx_DrawBatch.h"
#include "Gfx_DrawExtract.h"
#include "Gfx_FrameDrawStream.h"

// Backend-facing packet contract produced by Gfx.
// RenderCore should consume this packet instead of owning Gfx-side prep semantics.

struct Gfx_PassDrawPacket {
    // Draw order is already finalized by Gfx (opaque/transparent pass semantics preserved).
    std::vector< Gfx_DrawInstance > myDraws;
    // Batch runs are optional for bindless path but always populated by current stream builder.
    std::vector< Gfx_BatchRun > myBatchRuns;
};

struct Gfx_FrameRenderPacket {
    Gfx_PassDrawPacket myOpaquePass;
    Gfx_PassDrawPacket myTransparentPass;
    size_t             myDrawCountBeforeCull = 0;
};

void Gfx_BuildFrameRenderPacketFromStream( const Gfx_FrameDrawStreamOutput& aStreamOutput, Gfx_FrameRenderPacket& aOutPacket );

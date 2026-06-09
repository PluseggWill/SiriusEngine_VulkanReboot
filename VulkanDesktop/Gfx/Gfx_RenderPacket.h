#pragma once

#include <cstddef>
#include <vector>

#include "Gfx_DrawBatch.h"
#include "Gfx_DrawExtract.h"
#include "Gfx_FrameDrawStream.h"

// Backend-facing packet contract produced by Gfx.
// RenderCore should consume this packet instead of owning Gfx-side prep semantics.

struct Gfx_PassDrawPacket {
    // One forward sub-pass worth of draws (opaque or transparent). Sort/batch finalized in Gfx.
    std::vector< Gfx_DrawInstance > myDraws;
    // Batch runs are optional for bindless path but always populated by current stream builder.
    std::vector< Gfx_BatchRun > myBatchRuns;
    // Index of this pass's draws[0] within the view's frame draw-template partition.
    uint32_t myDrawBufferPassOffset = 0;
};

// CONTRACT: Vk_ScenePasses records myOpaquePass then myTransparentPass in one render pass (Stage 1 forward).
struct Gfx_FrameRenderPacket {
    Gfx_PassDrawPacket myOpaquePass;
    Gfx_PassDrawPacket myTransparentPass;
    size_t             myDrawCountBeforeCull = 0;
    // Base slot in per-frame indirect/template buffers for this view (multi-view partition).
    uint32_t myDrawBufferBaseIndex = 0;
};

// Slot index into per-frame indirect/template buffers (same partition as instance slab).
inline uint32_t Gfx_ComputeDrawBufferSlot( uint32_t aViewBaseIndex, uint32_t aPassOffset, uint32_t aDrawIndexInPass ) {
    return aViewBaseIndex + aPassOffset + aDrawIndexInPass;
}

void Gfx_BuildFrameRenderPacketFromStream( const Gfx_FrameDrawStreamOutput& aStreamOutput, Gfx_FrameRenderPacket& aOutPacket );

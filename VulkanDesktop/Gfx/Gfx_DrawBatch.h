#pragma once

#include <cstdint>
#include <vector>

#include "Gfx_DrawExtract.h"

// Opaque batching (S1): contiguous runs in a **sorted** draw list sharing the same bind bucket.

struct Gfx_BatchRun {
    uint32_t myFirstDrawIndex = 0;
    uint32_t myDrawCount      = 0;
    uint64_t myBatchKey       = 0;  // sort key with depth bucket cleared
};

// Sort key low 16 bits = depth bucket; upper bits = pipeline perm + material + mesh.
inline uint64_t Gfx_OpaqueBatchKey( uint64_t aSortKey ) {
    return aSortKey & ~uint64_t{ 0xFFFFu };
}

// Appends batch runs; clears aOut first. Input must already be opaque-sorted.
void Gfx_BuildOpaqueDrawBatches( const std::vector< Gfx_DrawInstance >& aSortedDraws, std::vector< Gfx_BatchRun >& aOut );

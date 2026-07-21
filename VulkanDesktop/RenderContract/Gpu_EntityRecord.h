#pragma once

#include "Gpu_DrawTemplate.h"

#include <cstdint>

#include <glm/glm.hpp>

// Per SoA slot: world AABB + draw-template fields for GPU cull input (std430-friendly).
// Indexed by entity slot; inactive slots use layerMask == 0.
struct Gpu_EntityRecord {
    glm::vec4               myBoundsMin{ 0.0f };
    glm::vec4               myBoundsMax{ 0.0f };
    uint32_t                myLayerMask     = 0;
    uint32_t                myRenderFlags   = 0;
    uint32_t                myLogicalMeshId = 0;
    uint32_t                myMaterialId    = 0;
    Gpu_DrawIndirectCommand myIndirect{};
    uint32_t                myPad[ 3 ]{};  // std430 array stride alignment
};

static_assert( sizeof( Gpu_EntityRecord ) == 80, "Gpu_EntityRecord std430 stride" );
static_assert( sizeof( Gpu_EntityRecord ) % 16 == 0, "Gpu_EntityRecord must be 16-byte aligned for std430 arrays" );

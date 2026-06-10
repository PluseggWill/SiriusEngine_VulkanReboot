#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "Gfx_DrawTemplate.h"
#include "Gfx_SceneSoA.h"

// Per SoA slot: world AABB + draw-template fields for GPU cull input (std430-friendly).
// Indexed by entity slot; inactive slots use layerMask == 0.
struct Gfx_EntityGpuRecord {
    glm::vec4               myBoundsMin{ 0.0f };
    glm::vec4               myBoundsMax{ 0.0f };
    uint32_t                myLayerMask     = 0;
    uint32_t                myRenderFlags   = 0;
    uint32_t                myLogicalMeshId = 0;
    uint32_t                myMaterialId    = 0;
    Gfx_DrawIndirectCommand myIndirect{};
    uint32_t                myPad[ 3 ]{};  // std430 array stride alignment
};

static_assert( sizeof( Gfx_EntityGpuRecord ) == 80, "Gfx_EntityGpuRecord std430 stride" );
static_assert( sizeof( Gfx_EntityGpuRecord ) % 16 == 0, "Gfx_EntityGpuRecord must be 16-byte aligned for std430 arrays" );

void Gfx_FillEntityGpuRecord( Gfx_EntityGpuRecord& aOut, const Gfx_SceneSoA& aScene, uint32_t aSlot, uint32_t aIndexCount );

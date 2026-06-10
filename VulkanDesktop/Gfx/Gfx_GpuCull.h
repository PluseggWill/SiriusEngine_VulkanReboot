#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_DrawCullSort.h"

class Gfx_SceneSoA;

// Push constants for EntityCull.comp — must match GLSL layout and Gfx_DrawCullSort frustum (proj * view).
struct Gfx_GpuCullPushConstants {
    glm::mat4 viewProj{};
    uint32_t  viewLayerMask  = 0xFFFFFFFFu;
    uint32_t  slotCount      = 0;
    uint32_t  outputBaseSlot = 0;
    uint32_t  pad            = 0;
};

static_assert( sizeof( Gfx_GpuCullPushConstants ) == 80, "Gfx_GpuCullPushConstants push constant size" );

// G1: CPU reference for EntityCull.comp visibility (GfxTests / no Vulkan).
bool Gfx_IsEntitySlotVisibleForGpuCull( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, uint32_t aSlot );

void Gfx_CollectCpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots );

void Gfx_CollectGpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots );

bool Gfx_AreCpuGpuCullVisibleSlotsEqual( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView );

#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../RenderContract/Gpu_CullPush.h"
#include "Gfx_DrawCullSort.h"

class Gfx_SceneSoA;

// G1: CPU reference for EntityCull.comp visibility (GfxTests / no Vulkan).
bool Gfx_IsEntitySlotVisibleForGpuCull( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, uint32_t aSlot );

void Gfx_CollectCpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots );

void Gfx_CollectGpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots );

bool Gfx_AreCpuGpuCullVisibleSlotsEqual( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView );

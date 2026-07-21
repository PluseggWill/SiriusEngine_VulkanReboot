#pragma once

#include <cstdint>

#include <glm/glm.hpp>

// Push constants for EntityCull.comp — must match GLSL layout and Gfx_DrawCullSort frustum (proj * view).
struct Gpu_CullPushConstants {
    glm::mat4 viewProj{};
    uint32_t  viewLayerMask  = 0xFFFFFFFFu;
    uint32_t  slotCount      = 0;
    uint32_t  outputBaseSlot = 0;
    uint32_t  pad            = 0;
};

static_assert( sizeof( Gpu_CullPushConstants ) == 80, "Gpu_CullPushConstants push constant size" );

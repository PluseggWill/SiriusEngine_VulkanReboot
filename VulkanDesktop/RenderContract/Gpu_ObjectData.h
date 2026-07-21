#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// std140 UBO slice in per-frame instance slab (Set 2 / UNIFORM_BUFFER_DYNAMIC — S1 verify task).
struct Gpu_ObjectData {
    alignas( 16 ) glm::mat4 model;
    alignas( 4 ) uint32_t materialIndex = 0;
    alignas( 4 ) uint32_t _pad0         = 0;
    alignas( 4 ) uint32_t _pad1         = 0;
    alignas( 4 ) uint32_t _pad2         = 0;
};

static_assert( sizeof( Gpu_ObjectData ) == 80, "Gpu_ObjectData must be std140-compatible (80 bytes)" );

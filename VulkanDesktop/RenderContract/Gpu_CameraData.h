#pragma once

#include <glm/glm.hpp>

// std140 UBO, set 0 / eVk_CameraBinding — current + previous view/proj for temporal reprojection.
struct Gpu_CameraData {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 prevViewProj;
    alignas( 16 ) glm::mat4 currViewProj;
    alignas( 16 ) glm::vec4 temporalJitterAndFlags;  // xy=jitter(px), z=historyValid(0/1), w=reserved
};

static_assert( sizeof( Gpu_CameraData ) == 272, "Gpu_CameraData must be std140-compatible (272 bytes)" );

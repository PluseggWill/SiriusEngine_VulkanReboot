#pragma once

#include "Vk_FrameData.h"

#include <cstdint>
#include <vector>

// In-flight frame sync + per-frame UBO/slab/descriptor slots.
struct Vk_FrameContext {
    uint32_t myFrameNumber  = 0;
    uint32_t myCurrentFrame = 0;

    std::vector< Vk_FrameData > myFrameDatas;
};

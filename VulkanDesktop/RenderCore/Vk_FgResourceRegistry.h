#pragma once

#include <cstdint>

enum class Vk_FgResourceId : uint8_t {
    GBufferAlbedo = 0,
    GBufferNormalRoughness,
    GBufferWorldPosition,
    GBufferDepth,
    SwapchainDepth,
    AoRaw,
    HdrHybrid,
    SwapchainColor,
    Count
};

struct Vk_FgResourceRegistry {
    static bool IsDepth( Vk_FgResourceId aId );
};

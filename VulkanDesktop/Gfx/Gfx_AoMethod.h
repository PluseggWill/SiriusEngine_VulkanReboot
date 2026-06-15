#pragma once

#include <cstdint>

// Screen-space AO algorithm selection (HybridDeferred AO pass).
// Add a method: enum value, shader SPIR-V path, pipeline slot in Vk_AoPass, ImGui label.
enum class Gfx_AoMethod : uint8_t {
    ClassicSsao = 0,  // 16-tap hemisphere SSAO (full-res)
    HbaoPlus,         // Horizon-based AO (half-res + depth-aware upsample)
    Count
};

inline const char* Gfx_AoMethodLabel( Gfx_AoMethod aMethod ) {
    switch ( aMethod ) {
    case Gfx_AoMethod::ClassicSsao:
        return "Classic SSAO";
    case Gfx_AoMethod::HbaoPlus:
        return "HBAO+";
    default:
        return "Unknown";
    }
}

// Half-res compute + upsample (HBAO+). Classic SSAO is full-res only.
inline bool Gfx_AoMethodUsesHalfResTarget( Gfx_AoMethod aMethod ) {
    return aMethod == Gfx_AoMethod::HbaoPlus;
}

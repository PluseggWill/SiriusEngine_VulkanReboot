#pragma once

#include <cstdint>

// Screen-space AO algorithm selection (HybridDeferred AO pass).
enum class Gpu_AoMethod : uint8_t { ClassicSsao = 0, HbaoPlus, Gtao, Count };

inline const char* Gpu_AoMethodLabel( Gpu_AoMethod aMethod ) {
    switch ( aMethod ) {
    case Gpu_AoMethod::ClassicSsao:
        return "Classic SSAO";
    case Gpu_AoMethod::HbaoPlus:
        return "HBAO+";
    case Gpu_AoMethod::Gtao:
        return "GTAO";
    default:
        return "Unknown";
    }
}

inline bool Gpu_AoMethodUsesHalfResTarget( Gpu_AoMethod aMethod ) {
    return aMethod == Gpu_AoMethod::HbaoPlus || aMethod == Gpu_AoMethod::Gtao;
}

#pragma once

#include <cstdint>

// Screen-space AO algorithm selection (HybridDeferred AO pass).
enum class GpuAoMethod : uint8_t { ClassicSsao = 0, HbaoPlus, Gtao, Count };

inline const char* GpuAoMethodLabel( GpuAoMethod aMethod ) {
    switch ( aMethod ) {
    case GpuAoMethod::ClassicSsao:
        return "Classic SSAO";
    case GpuAoMethod::HbaoPlus:
        return "HBAO+";
    case GpuAoMethod::Gtao:
        return "GTAO";
    default:
        return "Unknown";
    }
}

inline bool GpuAoMethodUsesHalfResTarget( GpuAoMethod aMethod ) {
    return aMethod == GpuAoMethod::HbaoPlus || aMethod == GpuAoMethod::Gtao;
}

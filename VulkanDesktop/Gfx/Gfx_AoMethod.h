#pragma once

#include "../RenderContract/GpuAoMethod.h"

using Gfx_AoMethod = GpuAoMethod;

inline const char* Gfx_AoMethodLabel( Gfx_AoMethod aMethod ) {
    return GpuAoMethodLabel( aMethod );
}

inline bool Gfx_AoMethodUsesHalfResTarget( Gfx_AoMethod aMethod ) {
    return GpuAoMethodUsesHalfResTarget( aMethod );
}

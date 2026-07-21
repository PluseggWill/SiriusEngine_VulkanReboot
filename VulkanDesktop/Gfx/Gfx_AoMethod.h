#pragma once

#include "../RenderContract/Gpu_AoMethod.h"

using Gfx_AoMethod = Gpu_AoMethod;

inline const char* Gfx_AoMethodLabel( Gfx_AoMethod aMethod ) {
    return Gpu_AoMethodLabel( aMethod );
}

inline bool Gfx_AoMethodUsesHalfResTarget( Gfx_AoMethod aMethod ) {
    return Gpu_AoMethodUsesHalfResTarget( aMethod );
}

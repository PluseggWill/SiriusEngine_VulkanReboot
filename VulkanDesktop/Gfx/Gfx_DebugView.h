#pragma once

#include <cstdint>

// Forward debug visualization (packed in Gpu_EnvironmentData.myFogDistance.w for lit shaders).
enum Gfx_DebugViewMode : uint32_t {
    Gfx_DebugViewMode_Lit         = 0,
    Gfx_DebugViewMode_Depth       = 1,
    Gfx_DebugViewMode_WorldNormal = 2,
    Gfx_DebugViewMode_ShadowMap   = 3,
    Gfx_DebugViewMode_Ao          = 4,
    Gfx_DebugViewMode_HiZ         = 5,
    Gfx_DebugViewMode_Ddgi        = 6,
    Gfx_DebugViewMode_MotionVec   = 7,
};

inline float Gfx_DebugViewModeToShaderPacked( Gfx_DebugViewMode aMode ) {
    return static_cast< float >( aMode );
}

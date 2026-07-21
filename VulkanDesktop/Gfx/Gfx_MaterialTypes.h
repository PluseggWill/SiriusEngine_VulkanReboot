#pragma once

#include "../RenderContract/Gpu_MaterialParams.h"

#include <cstdint>

#include <glm/glm.hpp>

// CPU/scene aliases for the shader-contract alpha mode (values owned by RenderContract).
enum Gfx_MaterialAlphaMode : uint32_t {
    Gfx_MaterialAlphaMode_Opaque = Gpu_MaterialAlphaMode_Opaque,
    Gfx_MaterialAlphaMode_Mask   = Gpu_MaterialAlphaMode_Mask,
    Gfx_MaterialAlphaMode_Blend  = Gpu_MaterialAlphaMode_Blend,
};

// CPU-side material surface (no Vulkan handles — lives in Gfx/).
struct Gfx_MaterialSurface {
    glm::vec4 myBaseColorFactor{ 1.0f };
    float     myRoughness     = 0.5f;
    float     myMetallic      = 0.0f;
    float     myAlpha         = 1.0f;
    uint32_t  myAlphaMode     = Gfx_MaterialAlphaMode_Opaque;
    bool      myIsTransparent = false;
};

inline Gpu_MaterialParams Gfx_MaterialSurfaceToGpuParams( const Gfx_MaterialSurface& aSurface ) {
    Gpu_MaterialParams params{};
    params.myBaseColorFactor = aSurface.myBaseColorFactor;
    params.myRoughness       = aSurface.myRoughness;
    params.myMetallic        = aSurface.myMetallic;
    params.myAlpha           = aSurface.myAlpha;
    params.myAlphaMode       = aSurface.myAlphaMode;
    return params;
}

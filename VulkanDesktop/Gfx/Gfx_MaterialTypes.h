#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

// Alpha mode for forward materials (matches MaterialData.alphaMode in lit shaders).
enum Gfx_MaterialAlphaMode : uint32_t {
    Gfx_MaterialAlphaMode_Opaque = 0,
    Gfx_MaterialAlphaMode_Mask   = 1,
    Gfx_MaterialAlphaMode_Blend  = 2,
};

// Forward debug visualization (packed in GpuEnvironmentData.myFogDistance.w for lit shaders).
enum Gfx_DebugViewMode : uint32_t {
    Gfx_DebugViewMode_Lit         = 0,
    Gfx_DebugViewMode_Depth       = 1,
    Gfx_DebugViewMode_WorldNormal = 2,
    Gfx_DebugViewMode_ShadowMap   = 3,
    Gfx_DebugViewMode_Ao          = 4,
    Gfx_DebugViewMode_HiZ         = 5,
    Gfx_DebugViewMode_Ddgi        = 6,
};

inline float Gfx_DebugViewModeToShaderPacked( Gfx_DebugViewMode aMode ) {
    return static_cast< float >( aMode );
}

// CPU-side material surface (no Vulkan handles — lives in Gfx/).
struct Gfx_MaterialSurface {
    glm::vec4 myBaseColorFactor{ 1.0f };
    float     myRoughness     = 0.5f;
    float     myMetallic      = 0.0f;
    float     myAlpha         = 1.0f;
    uint32_t  myAlphaMode     = Gfx_MaterialAlphaMode_Opaque;
    bool      myIsTransparent = false;
};

// std140, Set 1 binding 1 — must match MaterialData in TriangleFrag_Lit.frag (Stage 1 forward contract).
struct GpuMaterialParams {
    alignas( 16 ) glm::vec4 myBaseColorFactor{ 1.0f };
    alignas( 4 ) float myRoughness    = 0.5f;
    alignas( 4 ) float myMetallic     = 0.0f;
    alignas( 4 ) float myAlpha        = 1.0f;
    alignas( 4 ) uint32_t myAlphaMode = Gfx_MaterialAlphaMode_Opaque;
};

// std430 material table entry — must match GpuMaterialEntry in TriangleFrag_Lit_Bindless.frag (vec4 @ 32).
struct GpuMaterialTableEntry {
    uint32_t  myTextureIndex = 0;
    float     myRoughness    = 0.5f;
    float     myMetallic     = 0.0f;
    float     myAlpha        = 1.0f;
    uint32_t  myAlphaMode    = Gfx_MaterialAlphaMode_Opaque;
    uint32_t  _padStd430[ 3 ]{};  // std430: vec4 baseColorFactor starts at byte 32
    glm::vec4 myBaseColorFactor{ 1.0f };
};
static_assert( sizeof( GpuMaterialTableEntry ) == 48, "GpuMaterialTableEntry std430 size" );
static_assert( offsetof( GpuMaterialTableEntry, myBaseColorFactor ) == 32, "GpuMaterialTableEntry std430 vec4 offset" );

inline GpuMaterialParams Gfx_MaterialSurfaceToGpuParams( const Gfx_MaterialSurface& aSurface ) {
    GpuMaterialParams params{};
    params.myBaseColorFactor = aSurface.myBaseColorFactor;
    params.myRoughness       = aSurface.myRoughness;
    params.myMetallic        = aSurface.myMetallic;
    params.myAlpha           = aSurface.myAlpha;
    params.myAlphaMode       = aSurface.myAlphaMode;
    return params;
}

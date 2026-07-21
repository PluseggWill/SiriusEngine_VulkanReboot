#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

// Alpha mode for forward materials — matches MaterialData.alphaMode in lit shaders.
enum Gpu_MaterialAlphaMode : uint32_t {
    Gpu_MaterialAlphaMode_Opaque = 0,
    Gpu_MaterialAlphaMode_Mask   = 1,
    Gpu_MaterialAlphaMode_Blend  = 2,
};

// std140, Set 1 binding 1 — must match MaterialData in TriangleFrag_Lit.frag (Stage 1 forward contract).
struct Gpu_MaterialParams {
    alignas( 16 ) glm::vec4 myBaseColorFactor{ 1.0f };
    alignas( 4 ) float myRoughness    = 0.5f;
    alignas( 4 ) float myMetallic     = 0.0f;
    alignas( 4 ) float myAlpha        = 1.0f;
    alignas( 4 ) uint32_t myAlphaMode = Gpu_MaterialAlphaMode_Opaque;
};

// std430 material table entry — must match GpuMaterialEntry in TriangleFrag_Lit_Bindless.frag (vec4 @ 32).
struct Gpu_MaterialTableEntry {
    uint32_t  myTextureIndex = 0;
    float     myRoughness    = 0.5f;
    float     myMetallic     = 0.0f;
    float     myAlpha        = 1.0f;
    uint32_t  myAlphaMode    = Gpu_MaterialAlphaMode_Opaque;
    uint32_t  _padStd430[ 3 ]{};  // std430: vec4 baseColorFactor starts at byte 32
    glm::vec4 myBaseColorFactor{ 1.0f };
};

static_assert( sizeof( Gpu_MaterialTableEntry ) == 48, "Gpu_MaterialTableEntry std430 size" );
static_assert( offsetof( Gpu_MaterialTableEntry, myBaseColorFactor ) == 32, "Gpu_MaterialTableEntry std430 vec4 offset" );

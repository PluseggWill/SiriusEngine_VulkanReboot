#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_DrawCullSort.h"
#include "Gfx_SceneSoA.h"

// Extract phase (S1): SoA -> flat DrawInstance list. No Vulkan in this module.

struct Gfx_DrawInstance {
    uint64_t mySortKey               = 0;
    uint32_t myMeshId                = 0;
    uint32_t myMaterialId            = 0;
    uint32_t myInstanceDataOffset    = 0;
    uint32_t myPipelinePermutationId = 0;
    uint32_t myEntityIndex           = 0;
};

// Parallel arrays: index i pairs myVisibleEntityIndices[i] with myDrawInstances[i] (kept in sync through cull/sort).
struct Gfx_ExtractResult {
    std::vector< uint32_t >         myVisibleEntityIndices;
    std::vector< Gfx_DrawInstance > myDrawInstances;
};

struct Gfx_FrameExtract {
    Gfx_ExtractResult myOpaque;
    Gfx_ExtractResult myTransparent;
};

// Opaque sort key: (permSlot 16 | material 16 | mesh 16 | depthBucket 16).
// permSlot: Gfx_ShaderPermutation::EncodeSortKeyPermSlot(shaderPermId, materialTableGeneration) — see Gfx_ShaderPermutation.h.
uint64_t Gfx_PackOpaqueSortKey( uint32_t aPermSlot, uint32_t aMaterialId, uint32_t aMeshId, uint16_t aDepthBucket );

void Gfx_SetMaterialTableGenerationForExtract( uint16_t aGeneration );

uint16_t Gfx_ComputeDepthBucket( float aEyeSpaceZ );

// Reads SoA columns; splits opaque vs transparent lists. Does not call Vulkan.
void Gfx_ExtractDrawInstances( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, Gfx_FrameExtract& aOut );

float Gfx_ComputeEyeSpaceZ( const glm::mat4& aView, const glm::vec3& aWorldPosition );

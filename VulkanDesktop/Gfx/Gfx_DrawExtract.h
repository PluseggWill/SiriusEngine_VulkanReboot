#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// Extract phase (S1): scene entities -> flat DrawInstance list. No Vulkan in this module.

struct Gfx_StableEntityId {
    uint32_t myIndex       = 0;
    uint32_t myGeneration  = 0;
};

struct Gfx_SceneEntity {
    Gfx_StableEntityId myId{};
    uint32_t           myMeshId       = 0;
    uint32_t           myMaterialId   = 0;
    glm::mat4          myWorldTransform{ 1.0f };
};

struct Gfx_DrawInstance {
    uint64_t mySortKey              = 0;
    uint32_t myMeshId               = 0;
    uint32_t myMaterialId           = 0;
    uint32_t myInstanceDataOffset   = 0;
    uint32_t myPipelinePermutationId = 0;
    uint32_t myEntityIndex          = 0;
};

struct Gfx_ExtractViewParams {
    glm::mat4 myView{ 1.0f };
    glm::mat4 myProj{ 1.0f };
};

struct Gfx_ExtractResult {
    std::vector< uint32_t >       myVisibleEntityIndices;
    std::vector< Gfx_DrawInstance > myDrawInstances;
};

// Opaque sort key: (pipelinePerm 16 | material 16 | mesh 16 | depthBucket 16). Farther draws get larger depthBucket.
uint64_t Gfx_PackOpaqueSortKey( uint32_t aPipelinePermutationId, uint32_t aMaterialId, uint32_t aMeshId, uint16_t aDepthBucket );

uint16_t Gfx_ComputeDepthBucket( float aEyeSpaceZ );

// Reads scene entities; writes visible indices + draw instances. Does not call Vulkan.
void Gfx_ExtractDrawInstances( const std::vector< Gfx_SceneEntity >& someEntities, const Gfx_ExtractViewParams& aView, Gfx_ExtractResult& aOut );

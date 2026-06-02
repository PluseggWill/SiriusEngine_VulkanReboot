#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_SceneSoA.h"

struct Gfx_ExtractResult;
class Gfx_SceneSoA;

// Shared view params for extract + cull (S1 draw prep). No Vulkan.
struct Gfx_CullViewParams {
    glm::mat4 myView{ 1.0f };
    glm::mat4 myProj{ 1.0f };
    uint32_t  myViewLayerMask = 0xFFFFFFFFu;
};

struct Gfx_FrustumPlanes {
    glm::vec4 myPlanes[ 6 ]{};
};

Gfx_FrustumPlanes Gfx_BuildFrustumFromViewProj( const glm::mat4& aViewProj );

bool Gfx_IsBoundsVisible( const Gfx_Bounds& aBounds, const Gfx_FrustumPlanes& aFrustum );

// Compact inOut: drop draws outside frustum or failing layer mask (parallel arrays stay paired).
void Gfx_CullDrawInstancesInPlace( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, Gfx_ExtractResult& aInOut );

// Sort opaque draws by mySortKey; reorders myVisibleEntityIndices[i] with myDrawInstances[i].
void Gfx_SortOpaqueDrawInstances( Gfx_ExtractResult& aResult );

// Back-to-front by eye-space Z (ascending); tie-break materialId then entityIndex.
void Gfx_SortTransparentDrawInstances( Gfx_ExtractResult& aResult, const Gfx_SceneSoA& aScene, const glm::mat4& aView );

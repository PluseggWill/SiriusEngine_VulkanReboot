#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_DrawExtract.h"
#include "Gfx_SceneSoA.h"

// CPU cull + opaque sort (S1 submission). No Vulkan.

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

// Compact inOut: drop draws outside frustum or failing layer mask (uses draw.myEntityIndex → SoA bounds/layer).
void Gfx_CullDrawInstancesInPlace( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, Gfx_ExtractResult& aInOut );

// Sort ascending by Gfx_DrawInstance::mySortKey (opaque state order).
void Gfx_SortOpaqueDrawInstances( std::vector< Gfx_DrawInstance >& aDrawInstances );

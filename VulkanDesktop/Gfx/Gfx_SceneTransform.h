#pragma once

#include "Gfx_SceneSoA.h"

#include <glm/glm.hpp>

#include <vector>

// Flat-scene transform state (S2): source world data is authored/simulated in Gfx,
// then resolved into SoA world matrices before extract/cull/render prep.
struct Gfx_SceneTransformState {
    std::vector< glm::mat4 > mySourceWorldTransforms;
    std::vector< glm::mat4 > myResolvedWorldTransforms;

    void Clear();
};

void Gfx_ResolveFlatWorldTransforms( const Gfx_SceneTransformState& aTransforms, Gfx_SceneSoA& aScene );

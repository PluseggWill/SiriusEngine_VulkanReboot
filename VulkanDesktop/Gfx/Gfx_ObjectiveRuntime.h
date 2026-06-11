#pragma once

#include "Gfx_SceneDesc.h"

#include <glm/glm.hpp>

enum class Gfx_ObjectiveOutcome : uint8_t {
    Playing = 0,
    Won     = 1,
    Lost    = 2,
};

struct Gfx_ObjectiveRuntimeState {
    Gfx_ObjectiveOutcome myOutcome        = Gfx_ObjectiveOutcome::Playing;
    float                myElapsedSeconds = 0.0f;
    bool                 myLoggedOutcome  = false;
};

void Gfx_ResetObjectiveRuntime( Gfx_ObjectiveRuntimeState& aState );

void Gfx_TickObjectiveRuntime( const Gfx_SceneObjectiveDesc& aObjective, const glm::vec3& aCameraPosition, float aDeltaSeconds, Gfx_ObjectiveRuntimeState& aState );

void Gfx_BuildObjectiveHud( const Gfx_SceneObjectiveDesc& aObjective, const Gfx_ObjectiveRuntimeState& aState );

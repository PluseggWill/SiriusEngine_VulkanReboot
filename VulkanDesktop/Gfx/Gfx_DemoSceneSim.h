#pragma once

#include "../Util/Util_EngineConfig.h"
#include "Gfx_SceneTransform.h"

// Opt-in demo motion: when features.demoRotate is true, applies Z spin to source transforms.
// When false, resolved transforms mirror source (stable benchmark / reload baseline).

void Gfx_ResetDemoSceneSimTime();

void Gfx_TickDemoSceneTransforms( const Util_EngineConfig& aConfig, Gfx_SceneTransformState& aTransforms );

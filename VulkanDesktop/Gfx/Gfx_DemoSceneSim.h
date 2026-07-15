#pragma once

#include "Gfx_SceneTransform.h"

// Opt-in demo motion: when enabled, applies Z spin to source transforms.
// When false, resolved transforms mirror source (stable benchmark / reload baseline).

void Gfx_ResetDemoSceneSimTime();

void Gfx_TickDemoSceneTransforms( bool aDemoRotateEnabled, Gfx_SceneTransformState& aTransforms );

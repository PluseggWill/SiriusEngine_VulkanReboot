#pragma once

#include "Gfx_SceneTransform.h"

// Demo-only simulation step: update resolved world transforms from source transforms.
// Resolve into SoA is a separate phase.

void Gfx_TickDemoSceneTransforms( Gfx_SceneTransformState& aTransforms );

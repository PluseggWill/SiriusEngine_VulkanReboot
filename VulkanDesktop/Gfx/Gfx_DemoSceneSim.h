#pragma once

#include "../Util/Util_EngineConfig.h"
#include "Gfx_SceneTransform.h"

// Demo-only simulation step: update resolved world transforms from source transforms.
// Resolve into SoA is a separate phase.

void Gfx_TickDemoSceneTransforms( const Util_EngineConfig& aConfig, Gfx_SceneTransformState& aTransforms );

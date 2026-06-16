#pragma once

// Util_RenderDebugPanel - Stage 1 forward parity hooks: skip sub-passes, depth/normal debug view.

#include "../Gfx/Gfx_MaterialTypes.h"
#include "Util_EngineConfig.h"

struct GpuEnvironmentData;

namespace UtilRenderDebugPanel {

// Session-only toggles (not written to engine.json). Skip flags read in Vk_ScenePasses::RecordScene.
struct State {
    bool              mySkipOpaquePass      = false;
    bool              mySkipTransparentPass = false;
    bool              myLodEnabled          = false;
    Gfx_DebugViewMode myDebugViewMode       = Gfx_DebugViewMode_Lit;
    uint32_t          myHiZDebugMip         = 0u;
};

// Parent window/tab must already be open (no Begin/End here). Patches myFogDistance.w before UpdateEnvironment.
void BuildContents( const Util_EngineConfig& aConfig, State& aState, GpuEnvironmentData& anEnvironment, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws );

}  // namespace UtilRenderDebugPanel

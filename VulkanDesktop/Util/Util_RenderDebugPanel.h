#pragma once

// Util_RenderDebugPanel - Stage 1 forward parity hooks: skip sub-passes, depth/normal debug view.

#include "../RenderCore/Vk_Types.h"
#include "Util_EngineConfig.h"

struct GpuEnvironmentData;

namespace UtilRenderDebugPanel {

// Session-only toggles (not written to engine.json). Skip flags read in Vk_ScenePasses::RecordScene.
struct State {
    bool              mySkipOpaquePass      = false;
    bool              mySkipTransparentPass = false;
    Gfx_DebugViewMode myDebugViewMode       = Gfx_DebugViewMode_Lit;
};

// ImGui panel; patches myFogDistance.w for GpuEnvironmentData (see Gfx_DebugViewMode). Must run after
// Gfx_FrameDrawPrep::Build and before Vk_FrameUniformUploader::UpdateEnvironment in DrawFrameGpu.
void Build( const Util_EngineConfig& aConfig, State& aState, GpuEnvironmentData& anEnvironment, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws );

}  // namespace UtilRenderDebugPanel

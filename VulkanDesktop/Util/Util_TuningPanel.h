#pragma once

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../RenderContract/Gpu_EnvironmentData.h"
#include "Util_EngineConfig.h"
#include "Util_TuningPrefs.h"

#include <filesystem>

struct DebugUIState;

namespace UtilTuningPanel {

// Parent window/tab must already be open (no Begin/End here).
void BuildToolbar( const Util_EngineConfig& aConfig, Gpu_EnvironmentData& anEnvironment, Gfx_LightingSettings& aLighting, Gfx_AoSettings& anAo, Gfx_PostSettings& aPost,
                   DebugUIState& aDebugUI );

// After App_InitScenePresentation; no-op if user-tuning.json missing.
void LoadAndApplyIfPresent( const std::filesystem::path& aAssetRoot, Gpu_EnvironmentData& anEnvironment, Gfx_LightingSettings& aLighting, Gfx_AoSettings& anAo,
                            Gfx_PostSettings& aPost, DebugUIState& aDebugUI );

}  // namespace UtilTuningPanel

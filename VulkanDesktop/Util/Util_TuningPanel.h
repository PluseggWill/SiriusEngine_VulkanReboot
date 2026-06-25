#pragma once

#include "Util_EngineConfig.h"
#include "Util_TuningPrefs.h"

#include <filesystem>

class Vk_Renderer;
struct DebugUIState;

namespace UtilTuningPanel {
// Save / reset toolbar for Engine Debug window (parent Begin already open).
void BuildToolbar( const Util_EngineConfig& aConfig, Vk_Renderer& aCore, DebugUIState& aDebugUI );

// After App_InitScenePresentation; no-op if user-tuning.json missing.
void LoadAndApplyIfPresent( const std::filesystem::path& aAssetRoot, Vk_Renderer& aCore, DebugUIState& aDebugUI );
}  // namespace UtilTuningPanel

#pragma once

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../RenderContract/Gpu_EnvironmentData.h"
#include "Util_EngineConfig.h"
#include "Util_InputSnapshot.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <string>

class Vk_Renderer;

// Module: Util_TuningPrefs — serialize ImGui tuning to Config/user-tuning.json (gitignored).
// Load order: struct defaults → engine.json lighting → user-tuning.json (after scene env init).
// Does not persist Render-debug (debug view, skip pass) or scene selection.
namespace Util_TuningPrefs {

constexpr int kTuningSchemaVersion = 1;

struct ViewportToggles {
    bool mySunGizmo         = true;
    bool myDdgiVolumeBounds = true;
};

struct Snapshot {
    glm::vec4            myAmbientColor{ 0.24f, 0.26f, 0.30f, 1.0f };
    glm::vec4            mySunlightColor{ 1.15f, 1.12f, 1.02f, 2.5f };
    glm::vec4            mySunlightDirection{};
    Gfx_LightingSettings myLighting{};
    Gfx_AoSettings       myAo{};
    Gfx_PostSettings     myPost{};
    Util_CameraSettings  myCamera{};
    ViewportToggles      myViewport{};
};

std::filesystem::path DefaultPath( const std::filesystem::path& aAssetRoot );

Snapshot Capture( Vk_Renderer& aRenderer, const Util_CameraSettings& aCamera, const ViewportToggles& aViewport );
void     Apply( const Snapshot& aSnapshot, Vk_Renderer& aRenderer, Util_CameraSettings& aCamera, ViewportToggles& aViewport );

bool LoadFromFile( const std::filesystem::path& aPath, Snapshot& aOut );
void SaveToFile( const std::filesystem::path& aPath, const Snapshot& aSnapshot );

// Renderer + camera/viewport only; caller restores environment (App_ApplyDefaultEnvironment).
void ResetRendererTuning( const Util_EngineConfig& aConfig, Vk_Renderer& aRenderer, Util_CameraSettings& aCamera, ViewportToggles& aViewport );

}  // namespace Util_TuningPrefs

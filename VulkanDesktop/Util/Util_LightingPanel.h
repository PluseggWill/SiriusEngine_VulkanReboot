#pragma once

struct GpuEnvironmentData;
struct Gfx_AoSettings;
struct Gfx_LightingSettings;
class Vk_Camera;

namespace UtilLightingPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( GpuEnvironmentData& anEnvironment, Gfx_LightingSettings& aLightingSettings, Gfx_AoSettings& aAoSettings );

// Foreground overlay: world-space sun ray + view-relative HUD compass (call after ImGui::NewFrame panels).
void DrawViewportSunGizmo( const GpuEnvironmentData& anEnvironment, const Vk_Camera& aCamera );
}  // namespace UtilLightingPanel

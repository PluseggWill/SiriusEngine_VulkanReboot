#pragma once

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../RenderContract/GpuEnvironmentData.h"

class Vk_Camera;

namespace UtilLightingPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildSunContents( GpuEnvironmentData& anEnvironment, bool& aShowSunGizmo );
void BuildShadowIblContents( Gfx_LightingSettings& aLightingSettings );
void BuildDdgiContents( Gfx_LightingSettings& aLightingSettings, bool& aShowVolumeBounds );
void BuildAoContents( Gfx_AoSettings& aAoSettings );
void BuildContactSoftContents( Gfx_AoSettings& aAoSettings );

// Foreground overlay: world-space sun ray + view-relative HUD compass (call after ImGui::NewFrame panels).
void DrawViewportSunGizmo( const GpuEnvironmentData& anEnvironment, const Gfx_LightingSettings& aLightingSettings, const Vk_Camera& aCamera, bool aShowSunGizmo,
                           bool aShowDdgiVolumeBounds );
}  // namespace UtilLightingPanel

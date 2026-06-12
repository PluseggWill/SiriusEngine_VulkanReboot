#pragma once

struct GpuEnvironmentData;
struct Gfx_LightingSettings;

namespace UtilLightingPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( GpuEnvironmentData& anEnvironment, Gfx_LightingSettings& aLightingSettings );
}  // namespace UtilLightingPanel

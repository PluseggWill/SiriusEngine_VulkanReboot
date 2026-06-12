#pragma once

struct GpuEnvironmentData;

namespace UtilLightingPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( GpuEnvironmentData& anEnvironment );
}  // namespace UtilLightingPanel

#pragma once

struct Util_CameraSettings;

namespace UtilCameraPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( Util_CameraSettings& aSettings );
}  // namespace UtilCameraPanel

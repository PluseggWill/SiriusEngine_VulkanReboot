#pragma once

class Vk_Camera;
struct Util_CameraSettings;

namespace UtilCameraPanel {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( Util_CameraSettings& aSettings, const Vk_Camera& aFlyCamera );
}  // namespace UtilCameraPanel

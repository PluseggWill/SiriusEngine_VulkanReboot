#include "Util_CameraPanel.h"

#include "Util_InputSnapshot.h"

#include <imgui.h>

void UtilCameraPanel::BuildContents( Util_CameraSettings& aSettings ) {
    ImGui::SliderFloat( "Move speed", &aSettings.myMoveSpeed, 0.5f, 20.0f );
    ImGui::SliderFloat( "Mouse sensitivity", &aSettings.myMouseSensitivity, 0.0005f, 0.01f, "%.4f" );

    ImGui::Separator();
    ImGui::TextUnformatted( "WASD move, Q/E vertical" );
    ImGui::TextUnformatted( "Hold RMB to look" );
}

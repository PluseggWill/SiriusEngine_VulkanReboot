#include "Util_CameraPanel.h"

#include "Util_InputSnapshot.h"

#include <imgui.h>

void UtilCameraPanel::Build( Util_CameraSettings& aSettings ) {
    ImGui::SetNextWindowPos( ImVec2( 10.f, 420.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowBgAlpha( 0.9f );
    ImGui::Begin( "Camera", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

    ImGui::SliderFloat( "Move speed", &aSettings.myMoveSpeed, 0.5f, 20.0f );
    ImGui::SliderFloat( "Mouse sensitivity", &aSettings.myMouseSensitivity, 0.0005f, 0.01f, "%.4f" );

    ImGui::Separator();
    ImGui::TextUnformatted( "WASD move, Q/E vertical" );
    ImGui::TextUnformatted( "Hold RMB to look" );

    ImGui::End();
}

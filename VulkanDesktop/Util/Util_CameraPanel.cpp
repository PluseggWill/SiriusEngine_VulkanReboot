#include "Util_CameraPanel.h"

#include "../RenderCore/Vk_Camera.h"
#include "Util_InputSnapshot.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

#include <imgui.h>

namespace {

void DrawScreenArrow( ImDrawList* aDrawList, ImVec2 aFrom, ImVec2 aTo, ImU32 aColor, float aThickness ) {
    aDrawList->AddLine( aFrom, aTo, aColor, aThickness );

    const ImVec2 dir{ aTo.x - aFrom.x, aTo.y - aFrom.y };
    const float  len = std::sqrt( dir.x * dir.x + dir.y * dir.y );
    if ( len < 4.0f ) {
        return;
    }

    const ImVec2 unit{ dir.x / len, dir.y / len };
    const ImVec2 perp{ -unit.y, unit.x };
    const float  headLen = std::min( 14.0f, len * 0.25f );
    const ImVec2 tip     = aTo;
    const ImVec2 wingA{ tip.x - unit.x * headLen + perp.x * headLen * 0.45f, tip.y - unit.y * headLen + perp.y * headLen * 0.45f };
    const ImVec2 wingB{ tip.x - unit.x * headLen - perp.x * headLen * 0.45f, tip.y - unit.y * headLen - perp.y * headLen * 0.45f };
    aDrawList->AddTriangleFilled( tip, wingA, wingB, aColor );
}

void DrawCameraPoseDiagram( const glm::vec3& aForward ) {
    const glm::vec3 forward = glm::length( aForward ) > 0.0001f ? glm::normalize( aForward ) : glm::vec3( 0.0f, 1.0f, 0.0f );

    const float horizLen = std::sqrt( forward.x * forward.x + forward.y * forward.y );
    const float yawDeg   = glm::degrees( std::atan2( forward.y, forward.x ) );
    const float pitchDeg = glm::degrees( std::atan2( forward.z, horizLen ) );

    ImGui::Text( "Yaw: %.0f deg  Pitch: %.0f deg", yawDeg, pitchDeg );
    ImGui::TextDisabled( "Forward = normalize(look target - eye), Z-up fly camera" );

    const ImVec2 canvasSize( 220.0f, 96.0f );
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton( "CameraPoseCanvas", canvasSize );
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImU32 frameCol    = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const ImU32 lineCol     = ImGui::GetColorU32( ImGuiCol_TextDisabled );
    const ImU32 forwardCol  = IM_COL32( 120, 200, 255, 255 );
    const ImU32 positionCol = IM_COL32( 255, 255, 255, 220 );

    draw->AddRectFilled( canvasPos, ImVec2( canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y ), frameCol, 4.0f );

    // Left: top-down XY view of forward (camera at center dot).
    const ImVec2 topDownCenter{ canvasPos.x + canvasSize.x * 0.25f, canvasPos.y + canvasSize.y * 0.5f };
    const float  topDownRadius = canvasSize.y * 0.36f;
    draw->AddCircle( topDownCenter, topDownRadius, lineCol, 0, 1.5f );
    draw->AddCircleFilled( topDownCenter, 4.0f, positionCol );
    draw->AddText( ImVec2( topDownCenter.x - topDownRadius, topDownCenter.y - topDownRadius - 14.0f ), lineCol, "Top (+Z)" );

    const ImVec2 topDownTip{ topDownCenter.x + forward.x * topDownRadius, topDownCenter.y - forward.y * topDownRadius };
    DrawScreenArrow( draw, topDownCenter, topDownTip, forwardCol, 2.5f );

    // Right: side elevation of forward.
    const ImVec2 sideOrigin{ canvasPos.x + canvasSize.x * 0.58f, canvasPos.y + canvasSize.y * 0.78f };
    const float  sideHorizScale = canvasSize.x * 0.16f;
    const float  sideVertScale  = canvasSize.y * 0.55f;
    draw->AddLine( ImVec2( sideOrigin.x - sideHorizScale, sideOrigin.y ), ImVec2( sideOrigin.x + sideHorizScale, sideOrigin.y ), lineCol, 1.5f );
    draw->AddLine( sideOrigin, ImVec2( sideOrigin.x, sideOrigin.y - sideVertScale ), lineCol, 1.5f );
    draw->AddText( ImVec2( sideOrigin.x + sideHorizScale + 4.0f, sideOrigin.y - 10.0f ), lineCol, "horiz" );
    draw->AddText( ImVec2( sideOrigin.x + 4.0f, sideOrigin.y - sideVertScale - 2.0f ), lineCol, "+Z" );
    draw->AddCircleFilled( sideOrigin, 4.0f, positionCol );

    const ImVec2 sideTip{ sideOrigin.x + horizLen * sideHorizScale, sideOrigin.y - forward.z * sideVertScale };
    DrawScreenArrow( draw, sideOrigin, sideTip, forwardCol, 2.5f );
}

}  // namespace

void UtilCameraPanel::BuildContents( Util_CameraSettings& aSettings, const Vk_Camera& aFlyCamera ) {
    ImGui::SliderFloat( "Move speed", &aSettings.myMoveSpeed, 0.5f, 20.0f );
    ImGui::SliderFloat( "Mouse sensitivity", &aSettings.myMouseSensitivity, 0.0005f, 0.01f, "%.4f" );

    ImGui::Separator();
    ImGui::TextUnformatted( "WASD move, Q/E vertical" );
    ImGui::TextUnformatted( "Hold RMB to look" );

    ImGui::Separator();
    ImGui::TextUnformatted( "Primary fly camera (main view)" );

    const glm::vec3 eye     = aFlyCamera.myEye;
    const glm::vec3 target  = aFlyCamera.myCenter;
    const glm::vec3 forward = glm::length( target - eye ) > 0.0001f ? glm::normalize( target - eye ) : glm::vec3( 0.0f, 1.0f, 0.0f );

    ImGui::Text( "Eye position: ( %.2f, %.2f, %.2f )", eye.x, eye.y, eye.z );
    ImGui::Text( "Look target:  ( %.2f, %.2f, %.2f )", target.x, target.y, target.z );
    ImGui::Text( "Forward:      ( %.2f, %.2f, %.2f )", forward.x, forward.y, forward.z );
    ImGui::TextDisabled( "FOV %.0f deg  near %.2f  far %.0f", aFlyCamera.myFov, aFlyCamera.myNear, aFlyCamera.myFar );

    DrawCameraPoseDiagram( forward );
}

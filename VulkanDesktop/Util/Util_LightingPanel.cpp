#include "Util_LightingPanel.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../RenderCore/Vk_Camera.h"
#include "../RenderCore/Vk_Types.h"
#include "Util_Logger.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

#include <imgui.h>

namespace {

bool sShowViewportSunGizmo = true;

bool ProjectWorldToScreen( const glm::mat4& aView, const glm::mat4& aProj, const glm::vec2& aViewportSize, const glm::vec3& aWorldPos, ImVec2& aOutScreen ) {
    const glm::vec4 clip = aProj * aView * glm::vec4( aWorldPos, 1.0f );
    if ( clip.w <= 0.0001f ) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3( clip ) / clip.w;
    if ( ndc.x < -1.05f || ndc.x > 1.05f || ndc.y < -1.05f || ndc.y > 1.05f || ndc.z < -0.05f || ndc.z > 1.05f ) {
        return false;
    }

    aOutScreen.x = ( ndc.x * 0.5f + 0.5f ) * aViewportSize.x;
    aOutScreen.y = ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * aViewportSize.y;
    return true;
}

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

void DrawSunDirectionDiagram( const glm::vec3& aSunDirTowardLight ) {
    const glm::vec3 sunDir = glm::length( aSunDirTowardLight ) > 0.0001f ? glm::normalize( aSunDirTowardLight ) : glm::vec3( 0.0f, 0.0f, 1.0f );

    const float horizLen     = std::sqrt( sunDir.x * sunDir.x + sunDir.y * sunDir.y );
    const float azimuthDeg   = glm::degrees( std::atan2( sunDir.y, sunDir.x ) );
    const float elevationDeg = glm::degrees( std::atan2( sunDir.z, horizLen ) );

    ImGui::Text( "Azimuth: %.0f deg  Elevation: %.0f deg", azimuthDeg, elevationDeg );
    ImGui::TextDisabled( "Direction = surface toward sun (same as shadow / direct light L)" );

    const ImVec2 canvasSize( 220.0f, 96.0f );
    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton( "SunDirCanvas", canvasSize );
    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImU32 frameCol = ImGui::GetColorU32( ImGuiCol_FrameBg );
    const ImU32 lineCol  = ImGui::GetColorU32( ImGuiCol_TextDisabled );
    const ImU32 sunCol   = IM_COL32( 255, 210, 80, 255 );
    draw->AddRectFilled( canvasPos, ImVec2( canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y ), frameCol, 4.0f );

    // Left: top-down XY (Z-up). +X right, +Y up on canvas.
    const ImVec2 topDownCenter{ canvasPos.x + canvasSize.x * 0.25f, canvasPos.y + canvasSize.y * 0.5f };
    const float  topDownRadius = canvasSize.y * 0.36f;
    draw->AddCircle( topDownCenter, topDownRadius, lineCol, 0, 1.5f );
    draw->AddText( ImVec2( topDownCenter.x - topDownRadius, topDownCenter.y - topDownRadius - 14.0f ), lineCol, "Top (+Z)" );

    const ImVec2 topDownTip{ topDownCenter.x + sunDir.x * topDownRadius, topDownCenter.y - sunDir.y * topDownRadius };
    DrawScreenArrow( draw, topDownCenter, topDownTip, sunCol, 2.5f );

    // Right: side elevation (horizontal vs +Z).
    const ImVec2 sideOrigin{ canvasPos.x + canvasSize.x * 0.58f, canvasPos.y + canvasSize.y * 0.78f };
    const float  sideHorizScale = canvasSize.x * 0.16f;
    const float  sideVertScale  = canvasSize.y * 0.55f;
    draw->AddLine( ImVec2( sideOrigin.x - sideHorizScale, sideOrigin.y ), ImVec2( sideOrigin.x + sideHorizScale, sideOrigin.y ), lineCol, 1.5f );
    draw->AddLine( sideOrigin, ImVec2( sideOrigin.x, sideOrigin.y - sideVertScale ), lineCol, 1.5f );
    draw->AddText( ImVec2( sideOrigin.x + sideHorizScale + 4.0f, sideOrigin.y - 10.0f ), lineCol, "horiz" );
    draw->AddText( ImVec2( sideOrigin.x + 4.0f, sideOrigin.y - sideVertScale - 2.0f ), lineCol, "+Z" );

    const ImVec2 sideTip{ sideOrigin.x + horizLen * sideHorizScale, sideOrigin.y - sunDir.z * sideVertScale };
    DrawScreenArrow( draw, sideOrigin, sideTip, sunCol, 2.5f );
}

void LogLightingSettingsIfChanged( const Gfx_LightingSettings& aSettings ) {
    static bool  sInitialized = false;
    static bool  sShadows     = true;
    static bool  sIbl         = true;
    static float sIntensity   = 1.0f;
    const bool   changed      = !sInitialized || sShadows != aSettings.myShadowsEnabled || sIbl != aSettings.myIblEnabled || sIntensity != aSettings.myIblIntensity;
    if ( !changed ) {
        return;
    }

    sInitialized = true;
    sShadows     = aSettings.myShadowsEnabled;
    sIbl         = aSettings.myIblEnabled;
    sIntensity   = aSettings.myIblIntensity;
    UtilLogger::Info( "LIGHTING", std::string( "shadows=" ) + ( sShadows ? "1" : "0" ) + " ibl=" + ( sIbl ? "1" : "0" ) + " iblIntensity=" + std::to_string( sIntensity ) );
}

}  // namespace

void UtilLightingPanel::BuildContents( GpuEnvironmentData& anEnvironment, Gfx_LightingSettings& aLightingSettings, Gfx_AoSettings& aAoSettings,
                                       Gfx_PostSettings& aPostSettings ) {
    ImGui::ColorEdit3( "Ambient", &anEnvironment.myAmbientColor.x );
    ImGui::ColorEdit3( "Sun color", &anEnvironment.mySunlightColor.x );
    ImGui::DragFloat3( "Sun direction", &anEnvironment.mySunlightDirection.x, 0.02f, -1.f, 1.f );

    if ( ImGui::Button( "Normalize sun direction" ) ) {
        const glm::vec3 dir = glm::vec3( anEnvironment.mySunlightDirection );
        if ( glm::dot( dir, dir ) > 0.0001f ) {
            anEnvironment.mySunlightDirection = glm::vec4( glm::normalize( dir ), 0.f );
        }
    }

    ImGui::Checkbox( "Viewport sun gizmo", &sShowViewportSunGizmo );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Yellow arrow in the 3D view: world sun direction from look target.\nHUD compass: sun bearing relative to fly camera." );
    }

    DrawSunDirectionDiagram( glm::vec3( anEnvironment.mySunlightDirection ) );

    ImGui::Separator();
    ImGui::Checkbox( "Shadows enabled", &aLightingSettings.myShadowsEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Directional shadow map (2048). Auto-disabled when sun is below horizon (Z-up)." );
    }
    ImGui::Checkbox( "IBL enabled", &aLightingSettings.myIblEnabled );
    ImGui::SliderFloat( "IBL intensity", &aLightingSettings.myIblIntensity, 0.f, 3.f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "When IBL is off, ambient color scales the legacy fallback." );
    }

    ImGui::Separator();
    ImGui::Text( "Screen-space AO" );
    ImGui::Checkbox( "AO enabled", &aAoSettings.myEnabled );
    ImGui::SliderFloat( "AO radius", &aAoSettings.myRadius, 0.05f, 2.0f );
    ImGui::SliderFloat( "AO bias", &aAoSettings.myBias, 0.001f, 0.1f );
    ImGui::SliderFloat( "AO intensity", &aAoSettings.myIntensity, 0.f, 1.f );
    ImGui::SliderFloat( "AO power", &aAoSettings.myPower, 0.5f, 4.f );

    ImGui::Separator();
    ImGui::Text( "Post-process" );
    ImGui::Checkbox( "Tonemap enabled", &aPostSettings.myTonemapEnabled );
    ImGui::SliderFloat( "Exposure", &aPostSettings.myExposure, 0.1f, 4.f );
    const char* tonemapModes[] = { "Reinhard", "ACES" };
    int         tonemapMode    = aPostSettings.myTonemapMode;
    if ( ImGui::Combo( "Tonemap mode", &tonemapMode, tonemapModes, 2 ) ) {
        aPostSettings.myTonemapMode = tonemapMode;
    }
    ImGui::Checkbox( "Bloom enabled", &aPostSettings.myBloomEnabled );
    ImGui::SliderFloat( "Bloom threshold", &aPostSettings.myBloomThreshold, 0.5f, 4.f );
    ImGui::SliderFloat( "Bloom intensity", &aPostSettings.myBloomIntensity, 0.f, 2.f );

    ImGui::Separator();
    ImGui::TextDisabled( "Specular / shininess: unused under PBR (material roughness/metallic)" );
    ImGui::BeginDisabled();
    ImGui::SliderFloat( "Specular strength (legacy)", &anEnvironment.myFogDistance.x, 0.f, 2.f );
    ImGui::SliderFloat( "Shininess (legacy)", &anEnvironment.myFogDistance.y, 1.f, 256.f );
    ImGui::EndDisabled();
    ImGui::SliderFloat( "Gfx_Texture blend", &anEnvironment.myFogDistance.z, 0.f, 1.f );

    LogLightingSettingsIfChanged( aLightingSettings );
}

void UtilLightingPanel::DrawViewportSunGizmo( const GpuEnvironmentData& anEnvironment, const Vk_Camera& aCamera ) {
    if ( !sShowViewportSunGizmo ) {
        return;
    }

    const glm::vec3 sunDir = glm::length( glm::vec3( anEnvironment.mySunlightDirection ) ) > 0.0001f ? glm::normalize( glm::vec3( anEnvironment.mySunlightDirection ) )
                                                                                                     : glm::vec3( 0.0f, 0.0f, 1.0f );

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2         vpPos    = viewport->WorkPos;
    const ImVec2         vpSize   = viewport->WorkSize;
    ImDrawList*          draw     = ImGui::GetForegroundDrawList();

    // World-space ray from fly look target along sun direction (visible in scene).
    const float     rayLen = std::max( 0.75f, glm::length( aCamera.myEye - aCamera.myCenter ) * 2.0f );
    const glm::vec3 anchor = aCamera.myCenter;
    const glm::vec3 sunEnd = anchor + sunDir * rayLen;

    ImVec2     anchorScreen{};
    ImVec2     sunEndScreen{};
    const bool anchorOk = ProjectWorldToScreen( aCamera.myView, aCamera.myProj, glm::vec2( vpSize.x, vpSize.y ), anchor, anchorScreen );
    const bool sunEndOk = ProjectWorldToScreen( aCamera.myView, aCamera.myProj, glm::vec2( vpSize.x, vpSize.y ), sunEnd, sunEndScreen );
    if ( anchorOk && sunEndOk ) {
        anchorScreen.x += vpPos.x;
        anchorScreen.y += vpPos.y;
        sunEndScreen.x += vpPos.x;
        sunEndScreen.y += vpPos.y;
        const ImU32 sunCol = IM_COL32( 255, 210, 80, 230 );
        draw->AddCircleFilled( anchorScreen, 4.0f, IM_COL32( 255, 255, 255, 180 ) );
        DrawScreenArrow( draw, anchorScreen, sunEndScreen, sunCol, 3.0f );
        draw->AddText( ImVec2( sunEndScreen.x + 6.0f, sunEndScreen.y - 8.0f ), sunCol, "Sun" );
    }

    // HUD: sun bearing in camera view (top-right of main viewport).
    const glm::vec3 sunView = glm::normalize( glm::mat3( aCamera.myView ) * sunDir );
    const ImVec2    hudCenter{ vpPos.x + vpSize.x - 56.0f, vpPos.y + 56.0f };
    const float     hudRadius = 34.0f;
    const ImU32     hudFrame  = IM_COL32( 20, 20, 24, 180 );
    const ImU32     hudLine   = IM_COL32( 180, 180, 190, 200 );
    const ImU32     hudSun    = IM_COL32( 255, 210, 80, 255 );

    draw->AddCircleFilled( hudCenter, hudRadius + 4.0f, hudFrame );
    draw->AddCircle( hudCenter, hudRadius, hudLine, 0, 1.5f );
    draw->AddLine( ImVec2( hudCenter.x - hudRadius, hudCenter.y ), ImVec2( hudCenter.x + hudRadius, hudCenter.y ), hudLine, 1.0f );
    draw->AddLine( ImVec2( hudCenter.x, hudCenter.y - hudRadius ), ImVec2( hudCenter.x, hudCenter.y + hudRadius ), hudLine, 1.0f );
    draw->AddText( ImVec2( hudCenter.x - 18.0f, hudCenter.y + hudRadius + 6.0f ), hudLine, "Sun HUD" );

    if ( sunView.z >= -0.05f ) {
        const ImVec2 hudTip{ hudCenter.x + sunView.x * hudRadius, hudCenter.y - sunView.y * hudRadius };
        DrawScreenArrow( draw, hudCenter, hudTip, hudSun, 2.5f );
    }
    else {
        draw->AddText( ImVec2( hudCenter.x - 22.0f, hudCenter.y - 6.0f ), IM_COL32( 200, 120, 80, 255 ), "behind" );
    }
}

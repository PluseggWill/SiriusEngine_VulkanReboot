#include "Util_LightingPanel.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../RenderCore/Vk_Camera.h"
#include "../RenderCore/Vk_Types.h"
#include "Util_Logger.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <utility>

#include <imgui.h>

namespace {

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

void DrawDdgiVolumeBounds( ImDrawList* aDrawList, const ImVec2& aViewportPos, const ImVec2& aViewportSize, const Vk_Camera& aCamera,
                           const Gfx_LightingSettings& aLightingSettings ) {
    const glm::vec3 minP = aLightingSettings.myDdgiVolumeCenter - aLightingSettings.myDdgiVolumeExtents;
    const glm::vec3 maxP = aLightingSettings.myDdgiVolumeCenter + aLightingSettings.myDdgiVolumeExtents;
    if ( maxP.x <= minP.x || maxP.y <= minP.y || maxP.z <= minP.z ) {
        return;
    }

    const std::array< glm::vec3, 8 > corners = {
        glm::vec3( minP.x, minP.y, minP.z ), glm::vec3( maxP.x, minP.y, minP.z ), glm::vec3( maxP.x, maxP.y, minP.z ), glm::vec3( minP.x, maxP.y, minP.z ),
        glm::vec3( minP.x, minP.y, maxP.z ), glm::vec3( maxP.x, minP.y, maxP.z ), glm::vec3( maxP.x, maxP.y, maxP.z ), glm::vec3( minP.x, maxP.y, maxP.z ),
    };
    const std::array< std::pair< int, int >, 12 > edges = {
        std::pair< int, int >( 0, 1 ), std::pair< int, int >( 1, 2 ), std::pair< int, int >( 2, 3 ), std::pair< int, int >( 3, 0 ),
        std::pair< int, int >( 4, 5 ), std::pair< int, int >( 5, 6 ), std::pair< int, int >( 6, 7 ), std::pair< int, int >( 7, 4 ),
        std::pair< int, int >( 0, 4 ), std::pair< int, int >( 1, 5 ), std::pair< int, int >( 2, 6 ), std::pair< int, int >( 3, 7 ),
    };

    std::array< ImVec2, 8 > projected{};
    std::array< bool, 8 >   visible{};
    for ( int i = 0; i < 8; ++i ) {
        visible[ i ] = ProjectWorldToScreen( aCamera.myView, aCamera.myProj, glm::vec2( aViewportSize.x, aViewportSize.y ), corners[ i ], projected[ i ] );
        if ( visible[ i ] ) {
            projected[ i ].x += aViewportPos.x;
            projected[ i ].y += aViewportPos.y;
        }
    }

    const ImU32 lineColor = IM_COL32( 80, 190, 255, 220 );
    for ( const auto& edge : edges ) {
        if ( visible[ edge.first ] && visible[ edge.second ] ) {
            aDrawList->AddLine( projected[ edge.first ], projected[ edge.second ], lineColor, 2.0f );
        }
    }

    if ( visible[ 6 ] ) {
        aDrawList->AddText( ImVec2( projected[ 6 ].x + 6.0f, projected[ 6 ].y - 6.0f ), lineColor, "DDGI Volume" );
    }
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

void UtilLightingPanel::BuildSunContents( GpuEnvironmentData& anEnvironment, bool& aShowSunGizmo ) {
    ImGui::ColorEdit3( "Ambient", &anEnvironment.myAmbientColor.x );
    ImGui::ColorEdit3( "Sun color", &anEnvironment.mySunlightColor.x );
    ImGui::DragFloat3( "Sun direction", &anEnvironment.mySunlightDirection.x, 0.02f, -1.f, 1.f );

    if ( ImGui::Button( "Normalize sun direction" ) ) {
        const glm::vec3 dir = glm::vec3( anEnvironment.mySunlightDirection );
        if ( glm::dot( dir, dir ) > 0.0001f ) {
            anEnvironment.mySunlightDirection = glm::vec4( glm::normalize( dir ), 0.f );
        }
    }

    ImGui::Checkbox( "Viewport sun gizmo", &aShowSunGizmo );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Yellow arrow in the 3D view: world sun direction from look target.\nHUD compass: sun bearing relative to fly camera." );
    }

    DrawSunDirectionDiagram( glm::vec3( anEnvironment.mySunlightDirection ) );
}

void UtilLightingPanel::BuildShadowIblContents( Gfx_LightingSettings& aLightingSettings ) {
    ImGui::Checkbox( "Shadows enabled", &aLightingSettings.myShadowsEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Directional shadow map (2048). Auto-disabled when sun is below horizon (Z-up)." );
    }
    ImGui::Checkbox( "IBL enabled", &aLightingSettings.myIblEnabled );
    ImGui::SliderFloat( "IBL intensity", &aLightingSettings.myIblIntensity, 0.f, 3.f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "When IBL is off, ambient color scales the ambient fallback." );
    }
    ImGui::SliderFloat( "IBL spec shadow min", &aLightingSettings.myIblSpecularShadowMin, 0.f, 1.f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Specular IBL in full sun shadow: mix(min, 1.0, sunShadow). Diffuse IBL unchanged." );
    }
    ImGui::Checkbox( "Specular occlusion (IBL)", &aLightingSettings.mySpecularOcclusionEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Roughness-aware specular occlusion from SSAO (Frostbite Lagarde). Attenuates distant cubemap reflections in occluded areas." );
    }
    ImGui::Checkbox( "Specular occlusion cones (GTAO)", &aLightingSettings.mySpecularOcclusionUseCones );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Bent-normal cone specular occlusion (requires AO method GTAO). Replaces Lagarde when enabled." );
    }

    ImGui::Separator();
    ImGui::Checkbox( "SSR enabled", &aLightingSettings.mySsrEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Hi-Z screen-space reflections blended over distant prefilter IBL (Frostbite reflection stack v0)." );
    }
    ImGui::SliderFloat( "SSR max roughness", &aLightingSettings.mySsrMaxRoughness, 0.05f, 1.0f );
    ImGui::SliderFloat( "SSR max distance", &aLightingSettings.mySsrMaxDistance, 5.0f, 120.0f );
    ImGui::SliderFloat( "SSR thickness", &aLightingSettings.mySsrThickness, 0.01f, 0.2f );
    int ssrSteps = static_cast< int >( aLightingSettings.mySsrMaxSteps );
    if ( ImGui::SliderInt( "SSR max steps", &ssrSteps, 8, 64 ) ) {
        aLightingSettings.mySsrMaxSteps = static_cast< uint32_t >( ssrSteps );
    }
    ImGui::SliderFloat( "SSR history depth reject", &aLightingSettings.mySsrHistoryDepthReject, 0.01f, 0.5f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Reprojection depth tolerance for temporal lit HDR SSR hits (sigma in NDC depth units)." );
    }

    ImGui::Separator();
    ImGui::Checkbox( "Local reflection probe", &aLightingSettings.myLocalReflectionProbeEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Parallax-corrected box probe (reuses prefilter cubemap). Disabled when DDGI is on." );
    }
    if ( aLightingSettings.myLocalReflectionProbeEnabled && !aLightingSettings.myDdgiEnabled ) {
        ImGui::SliderFloat( "Local probe intensity", &aLightingSettings.myLocalProbeIntensity, 0.0f, 3.0f );
        ImGui::DragFloat3( "Local probe center", &aLightingSettings.myLocalProbeCenter.x, 0.1f );
        ImGui::DragFloat3( "Local probe extents", &aLightingSettings.myLocalProbeExtents.x, 0.1f, 0.5f, 200.0f );
    }

    LogLightingSettingsIfChanged( aLightingSettings );
}

void UtilLightingPanel::BuildDdgiContents( Gfx_LightingSettings& aLightingSettings, bool& aShowVolumeBounds ) {
    ImGui::Checkbox( "DDGI enabled", &aLightingSettings.myDdgiEnabled );
    ImGui::SliderFloat( "DDGI intensity", &aLightingSettings.myDdgiIntensity, 0.0f, 2.0f );
    ImGui::SliderFloat( "DDGI history blend", &aLightingSettings.myDdgiHistoryBlend, 0.0f, 0.99f );
    ImGui::Checkbox( "DDGI staggered update", &aLightingSettings.myDdgiStaggeredUpdate );
    int ddgiBudget = static_cast< int >( aLightingSettings.myDdgiUpdateBudget );
    if ( ImGui::SliderInt( "DDGI update budget", &ddgiBudget, 1, 512 ) ) {
        aLightingSettings.myDdgiUpdateBudget = static_cast< uint32_t >( ddgiBudget );
    }
    int probeX = static_cast< int >( aLightingSettings.myDdgiProbeCountX );
    int probeY = static_cast< int >( aLightingSettings.myDdgiProbeCountY );
    int probeZ = static_cast< int >( aLightingSettings.myDdgiProbeCountZ );
    if ( ImGui::SliderInt( "DDGI probes X", &probeX, 2, 24 ) ) {
        aLightingSettings.myDdgiProbeCountX = static_cast< uint32_t >( probeX );
    }
    if ( ImGui::SliderInt( "DDGI probes Y", &probeY, 2, 16 ) ) {
        aLightingSettings.myDdgiProbeCountY = static_cast< uint32_t >( probeY );
    }
    if ( ImGui::SliderInt( "DDGI probes Z", &probeZ, 2, 24 ) ) {
        aLightingSettings.myDdgiProbeCountZ = static_cast< uint32_t >( probeZ );
    }
    ImGui::DragFloat3( "DDGI volume center", &aLightingSettings.myDdgiVolumeCenter.x, 0.1f );
    ImGui::DragFloat3( "DDGI volume extents", &aLightingSettings.myDdgiVolumeExtents.x, 0.1f, 0.5f, 200.0f );
    ImGui::SliderFloat( "DDGI debug overlay", &aLightingSettings.myDdgiDebugOverlay, 0.0f, 1.0f );

    ImGui::Separator();
    ImGui::Checkbox( "Viewport DDGI volume bounds", &aShowVolumeBounds );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Draw world-space DDGI volume AABB wireframe in the viewport." );
    }
}

void UtilLightingPanel::BuildAoContents( Gfx_AoSettings& aAoSettings ) {
    const char* labels[] = { Gfx_AoMethodLabel( Gfx_AoMethod::ClassicSsao ), Gfx_AoMethodLabel( Gfx_AoMethod::HbaoPlus ), Gfx_AoMethodLabel( Gfx_AoMethod::Gtao ) };
    int         method   = static_cast< int >( aAoSettings.myMethod );
    if ( ImGui::Combo( "AO method", &method, labels, IM_ARRAYSIZE( labels ) ) ) {
        aAoSettings.myMethod = static_cast< Gfx_AoMethod >( method );
    }
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Algorithm in Vk_AoPass — swap without changing deferred or contact-soft pass." );
    }

    ImGui::Checkbox( "AO enabled", &aAoSettings.myEnabled );
    ImGui::SliderFloat( "AO radius", &aAoSettings.myRadius, 0.05f, 2.0f );
    ImGui::SliderFloat( "AO bias", &aAoSettings.myBias, 0.001f, 0.1f );
    ImGui::SliderFloat( "AO normal-aware radius", &aAoSettings.myNormalAwareRadius, 0.0f, 1.0f );
    ImGui::Checkbox( "AO temporal enabled", &aAoSettings.myTemporalEnabled );
    ImGui::SliderFloat( "AO temporal blend", &aAoSettings.myTemporalBlend, 0.0f, 0.98f );
    ImGui::SliderFloat( "AO intensity", &aAoSettings.myIntensity, 0.f, 1.f );
    ImGui::SliderFloat( "AO power", &aAoSettings.myPower, 0.5f, 4.f );

    if ( aAoSettings.myMethod == Gfx_AoMethod::HbaoPlus ) {
        int dirs  = static_cast< int >( aAoSettings.myHbaoDirections );
        int steps = static_cast< int >( aAoSettings.myHbaoSteps );
        if ( ImGui::SliderInt( "HBAO directions", &dirs, 1, 8 ) ) {
            aAoSettings.myHbaoDirections = static_cast< uint32_t >( dirs );
        }
        if ( ImGui::SliderInt( "HBAO steps", &steps, 1, 8 ) ) {
            aAoSettings.myHbaoSteps = static_cast< uint32_t >( steps );
        }
        ImGui::SliderFloat( "Upsample depth edge", &aAoSettings.myUpsampleDepthSigma, 0.01f, 0.06f );
    }
    else if ( aAoSettings.myMethod == Gfx_AoMethod::Gtao ) {
        int slices = static_cast< int >( aAoSettings.myGtaoSlices );
        int steps  = static_cast< int >( aAoSettings.myGtaoStepsPerSlice );
        if ( ImGui::SliderInt( "GTAO slices", &slices, 1, 16 ) ) {
            aAoSettings.myGtaoSlices = static_cast< uint32_t >( slices );
        }
        if ( ImGui::SliderInt( "GTAO steps / slice", &steps, 1, 16 ) ) {
            aAoSettings.myGtaoStepsPerSlice = static_cast< uint32_t >( steps );
        }
        ImGui::SliderFloat( "GTAO falloff", &aAoSettings.myGtaoFalloff, 0.5f, 4.0f );
        ImGui::SliderFloat( "Upsample depth edge", &aAoSettings.myUpsampleDepthSigma, 0.01f, 0.06f );
    }
}

void UtilLightingPanel::BuildContactSoftContents( Gfx_AoSettings& aAoSettings ) {
    ImGui::TextDisabled( "Screen-space blur on AO and sun shadow edges (HybridDeferred)." );
    ImGui::Checkbox( "Contact soft enabled", &aAoSettings.myContactSoftEnabled );
    ImGui::SliderFloat( "Soft blur radius", &aAoSettings.myContactSoftBlurRadius, 1.f, 4.f );
    ImGui::SliderFloat( "Soft depth edge", &aAoSettings.myContactSoftDepthSigma, 0.01f, 0.06f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Bilateral depth threshold — lower = sharper edges, higher = softer blur across depth gaps." );
    }
}

void UtilLightingPanel::DrawViewportSunGizmo( const GpuEnvironmentData& anEnvironment, const Gfx_LightingSettings& aLightingSettings, const Vk_Camera& aCamera,
                                              bool aShowSunGizmo, bool aShowDdgiVolumeBounds ) {
    if ( !aShowSunGizmo ) {
        return;
    }

    const glm::vec3 sunDir = glm::length( glm::vec3( anEnvironment.mySunlightDirection ) ) > 0.0001f ? glm::normalize( glm::vec3( anEnvironment.mySunlightDirection ) )
                                                                                                     : glm::vec3( 0.0f, 0.0f, 1.0f );

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2         vpPos    = viewport->WorkPos;
    const ImVec2         vpSize   = viewport->WorkSize;
    ImDrawList*          draw     = ImGui::GetForegroundDrawList();

    if ( aShowDdgiVolumeBounds && aLightingSettings.myDdgiEnabled ) {
        DrawDdgiVolumeBounds( draw, vpPos, vpSize, aCamera, aLightingSettings );
    }

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

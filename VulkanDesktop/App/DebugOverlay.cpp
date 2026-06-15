// Pre-record ImGui panels (phase 2 peel). All debug UI orchestration lives here (App thread).
#include "DebugOverlay.h"

#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_ObjectiveRuntime.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "../RenderCore/Vk_Core.h"
#include "../RenderCore/Vk_FrameCpuPrepResult.h"
#include "../RenderCore/Vk_Types.h"
#include "../Util/Util_CameraPanel.h"
#include "../Util/Util_FrameStats.h"
#include "../Util/Util_LightingPanel.h"
#include "../Util/Util_PostProcessPanel.h"
#include "../Util/Util_RenderDebugPanel.h"
#include "../Util/Util_ScenePanel.h"
#include "../Util/Util_StatsOverlay.h"
#include "DebugUIState.h"
#include "WorldState.h"
#include <imgui.h>

namespace {

void BuildDebugMenuBar( DebugUIState& aDebugUI ) {
    if ( !ImGui::BeginMainMenuBar() ) {
        return;
    }

    if ( ImGui::BeginMenu( "View" ) ) {
        ImGui::MenuItem( "Performance", nullptr, &aDebugUI.myPanelVisibility.myShowPerformance );
        ImGui::MenuItem( "Engine Debug", nullptr, &aDebugUI.myPanelVisibility.myShowEngineDebug );
        ImGui::MenuItem( "Objective HUD", nullptr, &aDebugUI.myPanelVisibility.myShowObjectiveHud );
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void BuildPerformanceWindow( DebugUIState& aDebugUI, const Util_FrameStats& aStats ) {
    if ( !aDebugUI.myPanelVisibility.myShowPerformance ) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos( ImVec2( display.x - 10.f, 10.f ), ImGuiCond_FirstUseEver, ImVec2( 1.f, 0.f ) );
    ImGui::SetNextWindowBgAlpha( 0.85f );
    if ( ImGui::Begin( "Performance", &aDebugUI.myPanelVisibility.myShowPerformance ) ) {
        UtilStatsOverlay::BuildContents( aStats );
    }
    ImGui::End();
}

void BuildMultiViewContents( DebugUIState& aDebugUI, const WorldState& aWorld, const Vk_FrameCpuPrepResult& aPrep ) {
    ImGui::Checkbox( "Enable PiP", &aDebugUI.myMultiView.myEnablePiP );
    const bool hasSceneCameras = !aWorld.myLoadedScene.myCameras.empty();
    if ( !hasSceneCameras ) {
        ImGui::TextUnformatted( "No scene cameras in scene JSON." );
    }
    else {
        if ( aDebugUI.myMultiView.mySecondaryCameraIndex >= aWorld.myLoadedScene.myCameras.size() ) {
            aDebugUI.myMultiView.mySecondaryCameraIndex = 0;
        }
        const Gfx_SceneCameraEntry& selected = aWorld.myLoadedScene.myCameras[ aDebugUI.myMultiView.mySecondaryCameraIndex ];
        if ( ImGui::BeginCombo( "Secondary camera", selected.myId.c_str() ) ) {
            for ( uint32_t i = 0; i < static_cast< uint32_t >( aWorld.myLoadedScene.myCameras.size() ); ++i ) {
                const bool isSelected = i == aDebugUI.myMultiView.mySecondaryCameraIndex;
                if ( ImGui::Selectable( aWorld.myLoadedScene.myCameras[ i ].myId.c_str(), isSelected ) ) {
                    aDebugUI.myMultiView.mySecondaryCameraIndex = i;
                }
                if ( isSelected ) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::Text( "Active views: %u", aPrep.myActiveViewCount );
}

void BuildEngineDebugWindow( const Util_EngineConfig& aConfig, DebugUIState& aDebugUI, const WorldState& aWorld, GpuEnvironmentData& anEnvironment,
                             Gfx_LightingSettings& aLightingSettings, Gfx_AoSettings& aAoSettings, Gfx_PostSettings& aPostSettings, const Vk_FrameCpuPrepResult& aPrep,
                             const Vk_Camera& aFlyCamera ) {
    if ( !aDebugUI.myPanelVisibility.myShowEngineDebug ) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos( ImVec2( 10.f, display.y - 10.f ), ImGuiCond_FirstUseEver, ImVec2( 0.f, 1.f ) );
    ImGui::SetNextWindowSize( ImVec2( 440.f, 340.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowBgAlpha( 0.9f );
    if ( ImGui::Begin( "Engine Debug", &aDebugUI.myPanelVisibility.myShowEngineDebug ) ) {
        if ( ImGui::BeginTabBar( "EngineDebugTabs", ImGuiTabBarFlags_None ) ) {
            if ( ImGui::BeginTabItem( "Scene" ) ) {
                UtilScenePanel::BuildContents( aConfig, aDebugUI.myScenePanel );
                ImGui::EndTabItem();
            }
            if ( ImGui::BeginTabItem( "Render" ) ) {
                UtilRenderDebugPanel::BuildContents( aConfig, aDebugUI.myRenderDebug, anEnvironment, aPrep.myTotalOpaqueDraws, aPrep.myTotalTransparentDraws );
                aAoSettings.myHiZDebugMip = aDebugUI.myRenderDebug.myHiZDebugMip;
                ImGui::Separator();
                UtilLightingPanel::BuildContents( anEnvironment, aLightingSettings, aAoSettings );
                ImGui::EndTabItem();
            }
            if ( ImGui::BeginTabItem( "Post" ) ) {
                UtilPostProcessPanel::BuildContents( aConfig, aPostSettings );
                ImGui::EndTabItem();
            }
            if ( ImGui::BeginTabItem( "Camera" ) ) {
                UtilCameraPanel::BuildContents( aDebugUI.myCameraSettings, aFlyCamera );
                ImGui::EndTabItem();
            }
            if ( ImGui::BeginTabItem( "Views" ) ) {
                BuildMultiViewContents( aDebugUI, aWorld, aPrep );
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

}  // namespace

void BuildDebugOverlayPanels( const Util_EngineConfig& aConfig, DebugUIState& aDebugUI, const WorldState& aWorld, Vk_Core& aCore, const Vk_FrameCpuPrepResult& aPrep ) {
    BuildDebugMenuBar( aDebugUI );
    BuildPerformanceWindow( aDebugUI, aCore.myFrameStats );
    Gfx_BuildObjectiveHud( aWorld.myLoadedScene.myObjective, aDebugUI.myObjectiveRuntime, aDebugUI.myPanelVisibility.myShowObjectiveHud );
    BuildEngineDebugWindow( aConfig, aDebugUI, aWorld, aCore.GetEnvironmentData(), aCore.GetLightingSettings(), aCore.GetAoSettings(), aCore.GetPostSettings(), aPrep,
                            aCore.GetFlyCamera() );
    UtilLightingPanel::DrawViewportSunGizmo( aCore.GetEnvironmentData(), aCore.GetFlyCamera() );
}

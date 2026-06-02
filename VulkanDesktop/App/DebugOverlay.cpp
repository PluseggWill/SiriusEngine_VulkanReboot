// Pre-record ImGui panels (phase 2 peel). RenderDebug stays in Application (needs prep draw counts).
#include "DebugOverlay.h"

#include "../Gfx/Gfx_SceneDesc.h"
#include "../RenderCore/Vk_Core.h"
#include "../RenderCore/Vk_FrameCpuPrepResult.h"
#include "../Util/Util_CameraPanel.h"
#include "../Util/Util_ScenePanel.h"
#include "../Util/Util_StatsOverlay.h"
#include "DebugUIState.h"
#include "WorldState.h"
#include <imgui.h>

void BuildDebugOverlayPanels( DebugUIState& aDebugUI, const WorldState& aWorld, Vk_Core& aCore, const Vk_FrameCpuPrepResult& aPrep ) {
    UtilCameraPanel::Build( aDebugUI.myCameraSettings );
    UtilScenePanel::Build( aDebugUI.myScenePanel );

    if ( ImGui::Begin( "Multi-view", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
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
    ImGui::End();

    UtilStatsOverlay::Build( aCore.myFrameStats );
}

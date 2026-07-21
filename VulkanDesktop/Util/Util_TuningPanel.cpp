#include "Util_TuningPanel.h"

#include "../App/DebugUIState.h"
#include "../App/SceneCpuLoad.h"
#include "Util_Logger.h"

#include <exception>
#include <imgui.h>

namespace UtilTuningPanel {

namespace {

    Util_TuningPrefs::ViewportToggles ToViewportToggles( const DebugUIState::ViewportOverlays& aOverlays ) {
        return Util_TuningPrefs::ViewportToggles{ aOverlays.mySunGizmo, aOverlays.myDdgiVolumeBounds };
    }

    void FromViewportToggles( const Util_TuningPrefs::ViewportToggles& aToggles, DebugUIState::ViewportOverlays& aOut ) {
        aOut.mySunGizmo         = aToggles.mySunGizmo;
        aOut.myDdgiVolumeBounds = aToggles.myDdgiVolumeBounds;
    }

}  // namespace

void BuildToolbar( const Util_EngineConfig& aConfig, Gpu_EnvironmentData& anEnvironment, Gfx_LightingSettings& aLighting, Gfx_AoSettings& anAo, Gfx_PostSettings& aPost,
                   DebugUIState& aDebugUI ) {
    if ( ImGui::Button( "Save tuning" ) ) {
        try {
            const Util_TuningPrefs::Snapshot snap =
                Util_TuningPrefs::Capture( anEnvironment, aLighting, anAo, aPost, aDebugUI.myCameraSettings, ToViewportToggles( aDebugUI.myViewportOverlays ) );
            Util_TuningPrefs::SaveToFile( Util_TuningPrefs::DefaultPath( aConfig.GetAssetRoot() ), snap );
            UtilLogger::Info( "CONFIG", "Saved user tuning." );
        }
        catch ( const std::exception& e ) {
            UtilLogger::Error( "CONFIG", std::string( "Save tuning failed: " ) + e.what() );
        }
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Reset tuning" ) ) {
        Util_TuningPrefs::ViewportToggles viewport = ToViewportToggles( aDebugUI.myViewportOverlays );
        Util_TuningPrefs::ResetTuning( aConfig, aLighting, anAo, aPost, aDebugUI.myCameraSettings, viewport );
        FromViewportToggles( viewport, aDebugUI.myViewportOverlays );
        App_ApplyDefaultEnvironmentData( anEnvironment );
        UtilLogger::Info( "CONFIG", "Reset tuning to baseline." );
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "Config/user-tuning.json" );
}

void LoadAndApplyIfPresent( const std::filesystem::path& aAssetRoot, Gpu_EnvironmentData& anEnvironment, Gfx_LightingSettings& aLighting, Gfx_AoSettings& anAo,
                            Gfx_PostSettings& aPost, DebugUIState& aDebugUI ) {
    Util_TuningPrefs::ViewportToggles viewport = ToViewportToggles( aDebugUI.myViewportOverlays );
    Util_TuningPrefs::Snapshot        snap{};
    if ( !Util_TuningPrefs::LoadFromFile( Util_TuningPrefs::DefaultPath( aAssetRoot ), snap ) ) {
        return;
    }
    Util_TuningPrefs::Apply( snap, anEnvironment, aLighting, anAo, aPost, aDebugUI.myCameraSettings, viewport );
    FromViewportToggles( viewport, aDebugUI.myViewportOverlays );
    UtilLogger::Info( "CONFIG", std::string( "Applied user tuning: " ) + Util_TuningPrefs::DefaultPath( aAssetRoot ).string() );
}

}  // namespace UtilTuningPanel

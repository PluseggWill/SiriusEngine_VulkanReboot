#include "Util_ScenePanel.h"

#include "Util_EngineConfig.h"
#include "Util_Logger.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace {

std::string RepoRelativeScenePath( const std::filesystem::path& aAssetRoot, const std::filesystem::path& aJsonPath ) {
    std::error_code             ec;
    const std::filesystem::path rel = std::filesystem::relative( aJsonPath, aAssetRoot, ec );
    if ( ec ) {
        return aJsonPath.filename().string();
    }
    return rel.generic_string();
}

int FindIndex( const std::vector< std::string >& aPaths, const std::string& aTarget ) {
    const auto it = std::find( aPaths.begin(), aPaths.end(), aTarget );
    if ( it == aPaths.end() ) {
        return 0;
    }
    return static_cast< int >( std::distance( aPaths.begin(), it ) );
}

}  // namespace

namespace UtilScenePanel {

void RequestReload( State& aState, const std::string& aLogicalPath ) {
    aState.myReloadTargetPath = aLogicalPath;
    aState.myReloadRequested  = true;
}

void RefreshSceneList( const Util_EngineConfig& aConfig, State& aState ) {
    aState.myAvailableScenes.clear();

    const std::filesystem::path scenesDir = aConfig.GetAssetRoot() / "Data" / "Scenes";
    if ( !std::filesystem::exists( scenesDir ) || !std::filesystem::is_directory( scenesDir ) ) {
        aState.myLastError = "Scene directory missing: " + scenesDir.string();
        return;
    }

    const std::filesystem::path assetRoot = aConfig.GetAssetRoot();
    for ( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( scenesDir ) ) {
        if ( !entry.is_regular_file() ) {
            continue;
        }
        if ( entry.path().extension() != ".json" ) {
            continue;
        }
        aState.myAvailableScenes.push_back( RepoRelativeScenePath( assetRoot, entry.path() ) );
    }

    std::sort( aState.myAvailableScenes.begin(), aState.myAvailableScenes.end() );
    aState.mySelectedIndex = FindIndex( aState.myAvailableScenes, aState.myCurrentScenePath );
    aState.myLastError.clear();
}

void BuildContents( const Util_EngineConfig& aConfig, State& aState ) {
    ImGui::Text( "Current: %s", aState.myCurrentScenePath.empty() ? "(none)" : aState.myCurrentScenePath.c_str() );

    if ( ImGui::Button( "Refresh list" ) ) {
        RefreshSceneList( aConfig, aState );
    }

    if ( !aState.myLastError.empty() ) {
        ImGui::TextColored( ImVec4( 1.f, 0.4f, 0.3f, 1.f ), "%s", aState.myLastError.c_str() );
    }

    if ( aState.myAvailableScenes.empty() ) {
        ImGui::TextDisabled( "No .json under Data/Scenes/" );
    }
    else {
        if ( aState.mySelectedIndex < 0 || aState.mySelectedIndex >= static_cast< int >( aState.myAvailableScenes.size() ) ) {
            aState.mySelectedIndex = 0;
        }

        const char* preview = aState.myAvailableScenes[ static_cast< size_t >( aState.mySelectedIndex ) ].c_str();
        if ( ImGui::BeginCombo( "Scene file", preview ) ) {
            for ( int i = 0; i < static_cast< int >( aState.myAvailableScenes.size() ); ++i ) {
                const bool selected = ( i == aState.mySelectedIndex );
                if ( ImGui::Selectable( aState.myAvailableScenes[ static_cast< size_t >( i ) ].c_str(), selected ) ) {
                    aState.mySelectedIndex = i;
                }
                if ( selected ) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const bool sameAsCurrent =
            !aState.myCurrentScenePath.empty() && aState.myAvailableScenes[ static_cast< size_t >( aState.mySelectedIndex ) ] == aState.myCurrentScenePath;

        if ( sameAsCurrent ) {
            ImGui::BeginDisabled();
        }
        if ( ImGui::Button( "Load selected scene" ) ) {
            RequestReload( aState, aState.myAvailableScenes[ static_cast< size_t >( aState.mySelectedIndex ) ] );
        }
        if ( sameAsCurrent ) {
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled( "(already loaded)" );
        }

        if ( !aState.myCurrentScenePath.empty() && ImGui::Button( "Restart current (R)" ) ) {
            RequestReload( aState, aState.myCurrentScenePath );
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled( "assetVerify=%s (engine.json / startup)", aConfig.GetAssetVerifyPolicy() == Util_AssetVerifyPolicy::Strict ? "strict" : "warn" );
    ImGui::TextDisabled( "CLI reference: Docs/CLI.md" );
}

}  // namespace UtilScenePanel

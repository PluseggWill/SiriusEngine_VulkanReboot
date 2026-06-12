#pragma once

#include "Util_EngineConfig.h"

#include <string>
#include <vector>

// ImGui panel: list Data/Scenes/*.json and request in-process scene reload.
namespace UtilScenePanel {

struct State {
    std::string                myCurrentScenePath;
    std::vector< std::string > myAvailableScenes;
    int                        mySelectedIndex   = 0;
    bool                       myReloadRequested = false;
    std::string                myReloadTargetPath;
    std::string                myLastError;
};

// Scan <assetRoot>/Data/Scenes/*.json; paths are repo-relative (e.g. Data/Scenes/demo.json).
void RefreshSceneList( const Util_EngineConfig& aConfig, State& aState );

// Queue in-process reload (Application consumes on next frame).
void RequestReload( State& aState, const std::string& aLogicalPath );

// Parent window/tab must already be open (no Begin/End here).
void BuildContents( const Util_EngineConfig& aConfig, State& aState );

}  // namespace UtilScenePanel

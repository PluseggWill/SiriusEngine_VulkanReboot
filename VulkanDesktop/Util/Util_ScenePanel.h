#pragma once

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
void RefreshSceneList( State& aState );

void Build( State& aState );

}  // namespace UtilScenePanel

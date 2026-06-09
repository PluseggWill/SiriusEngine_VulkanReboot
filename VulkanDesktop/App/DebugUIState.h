#pragma once

#include "../Util/Util_InputSnapshot.h"
#include "../Util/Util_RenderDebugPanel.h"
#include "../Util/Util_ScenePanel.h"

// Application-owned ImGui / debug toggles (P1 peel phase 2). RenderCore reads skip flags via BindDebugUI.
struct DebugUIState {
    struct MultiViewState {
        bool     myEnablePiP            = true;
        uint32_t mySecondaryCameraIndex = 0;
    };

    MultiViewState              myMultiView;
    UtilScenePanel::State       myScenePanel;
    UtilRenderDebugPanel::State myRenderDebug;
    Util_CameraSettings         myCameraSettings;
};

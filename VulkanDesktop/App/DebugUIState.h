#pragma once

#include "../Gfx/Gfx_ObjectiveRuntime.h"
#include "../Util/Util_InputSnapshot.h"
#include "../Util/Util_RenderDebugPanel.h"
#include "../Util/Util_ScenePanel.h"

// Application-owned ImGui / debug toggles. Per-frame skip/LOD flags copied into Gfx_FrameDebugToggles for RenderCore.
struct DebugUIState {
    struct PanelVisibility {
        bool myShowPerformance  = true;
        bool myShowEngineDebug  = true;
        bool myShowObjectiveHud = true;
    };

    struct MultiViewState {
        bool     myEnablePiP            = false;
        uint32_t mySecondaryCameraIndex = 1;
    };

    PanelVisibility             myPanelVisibility;
    MultiViewState              myMultiView;
    UtilScenePanel::State       myScenePanel;
    UtilRenderDebugPanel::State myRenderDebug;
    Util_CameraSettings         myCameraSettings;
    Gfx_ObjectiveRuntimeState   myObjectiveRuntime;
};

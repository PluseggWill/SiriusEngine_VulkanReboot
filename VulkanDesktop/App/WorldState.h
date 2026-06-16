#pragma once

#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "../Gfx/Gfx_SceneTransform.h"
#include <cstdint>
#include <string>

// Application-owned scene CPU state. RenderCore reads via Gfx_FramePrepInput only.
struct WorldState {
    Gfx_SceneSoA            mySceneSoA;
    Gfx_LodTable            myLodTable;
    Gfx_LodState            myLodState;
    Gfx_SceneTransformState mySceneTransformState;
    Gfx_SceneIdTables       mySceneIdTables;
    Gfx_SceneDesc           myLoadedScene;
    std::string             myLogicalPath;
    uint32_t                myLodDebugLogicalMeshId = UINT32_MAX;
    bool                    myHasLoadedScene        = false;

    void ClearCpuSceneState();
};

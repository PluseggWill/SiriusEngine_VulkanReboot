#pragma once

#include "Gfx_SceneDesc.h"
#include "Gfx_SceneSoA.h"

// DTO for Renderer scene GPU load — App fills from WorldState; RenderCore must not include WorldState.
struct Gfx_SceneGpuLoadParams {
    const Gfx_SceneDesc*     mySceneDesc     = nullptr;
    const Gfx_SceneIdTables* mySceneIdTables = nullptr;
    Gfx_SceneSoA*            mySceneSoA      = nullptr;  // bounds writeback after mesh load
    std::string              myLogicalPath;
};

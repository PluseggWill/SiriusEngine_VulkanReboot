#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_DrawBatch.h"
#include "Gfx_DrawCullSort.h"
#include "Gfx_DrawExtract.h"
#include "Gfx_Lod.h"
#include "Gfx_SceneSoA.h"

// CPU draw-stream prep: extract → cull → LOD → sort → batch. No Vulkan.

struct Gfx_FrameDrawStreamParams {
    const Gfx_SceneSoA* myScene = nullptr;
    Gfx_CullViewParams  myView{};
    glm::vec3           myCameraEye{};
    glm::mat4           myCameraView{ 1.0f };
    const Gfx_LodTable* myLodTable              = nullptr;
    Gfx_LodState*       myLodState              = nullptr;
    bool                myLodEnabled            = false;
    uint32_t            myLodDebugLogicalMeshId = UINT32_MAX;
};

struct Gfx_FrameDrawStreamLogState {
    bool myLodLoggedOnce     = false;
    bool myBatchLoggedOnce   = false;
    bool myTransLoggedOnce   = false;
    bool myExtractLoggedOnce = false;
};

struct Gfx_FrameDrawStreamOutput {
    Gfx_FrameExtract            myExtract;
    std::vector< Gfx_BatchRun > myOpaqueBatchRuns;
    std::vector< Gfx_BatchRun > myTransparentBatchRuns;
    size_t                      myDrawCountBeforeCull = 0;
};

void Gfx_BuildFrameDrawStream( const Gfx_FrameDrawStreamParams& aParams, Gfx_FrameDrawStreamOutput& aOut, Gfx_FrameDrawStreamLogState& aLogs );

void Gfx_ResetFrameDrawStreamLogState( Gfx_FrameDrawStreamLogState& aLogs );

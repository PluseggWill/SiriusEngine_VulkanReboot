#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../Gfx/Gfx_DrawBatch.h"
#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_FrameDrawStream.h"
#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "Vk_Camera.h"
#include "Vk_FrameData.h"

// Per-frame draw prep: Gfx draw stream + instance slab CPU write (before vkCmd record).

struct Vk_FrameDrawPrepBuildParams {
    Gfx_SceneSoA*                myScene                 = nullptr;
    const Vk_Camera*             myCamera                = nullptr;
    const Gfx_LodTable*          myLodTable              = nullptr;
    Gfx_LodState*                myLodState              = nullptr;
    uint32_t                     myLodDebugLogicalMeshId = UINT32_MAX;
    uint32_t                     myCurrentFrame          = 0;
    std::vector< Vk_FrameData >* myFrameDatas            = nullptr;
    size_t                       myInstanceSlabStride    = 0;
};

class Vk_FrameDrawPrep {
public:
    Gfx_FrameExtract            myExtract;
    std::vector< Gfx_BatchRun > myOpaqueBatchRuns;
    std::vector< Gfx_BatchRun > myTransparentBatchRuns;
    size_t                      myDrawCountBeforeCull = 0;

    Gfx_FrameDrawStreamLogState myStreamLogs;

    void ClearFrameOutputs();
    void ResetLogState();

    // Returns false when instance slab overflow (record should be skipped).
    bool Build( const Vk_FrameDrawPrepBuildParams& aParams );

private:
    bool FillInstanceSlab( const Vk_FrameDrawPrepBuildParams& aParams );

    bool mySlabFillLoggedOnce          = false;
    bool myInstanceSlabOverflowLogged  = false;
};

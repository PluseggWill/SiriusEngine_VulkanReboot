#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../Gfx/Gfx_FrameDrawStream.h"
#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_SceneSoA.h"
#include "Vk_Camera.h"
#include "Vk_FrameData.h"

class Vk_ResourceTables;

// Per-frame draw prep: Gfx draw stream + instance slab CPU write (before vkCmd record).

struct Vk_FrameDrawPrepBuildParams {
    Gfx_SceneSoA*                myScene                  = nullptr;
    const Vk_Camera*             myCamera                 = nullptr;
    const Gfx_LodTable*          myLodTable               = nullptr;
    Gfx_LodState*                myLodState               = nullptr;
    bool                         myLodEnabled             = false;
    uint32_t                     myLodDebugLogicalMeshId  = UINT32_MAX;
    uint32_t                     myCurrentFrame           = 0;
    std::vector< Vk_FrameData >* myFrameDatas             = nullptr;
    size_t                       myInstanceSlabStride     = 0;
    size_t                       myInstanceSlabBaseOffset = 0;
    uint32_t                     myInstanceSlabMaxEntries = 0;
    uint32_t                     myDrawBufferBaseIndex    = 0;
    uint32_t                     myDrawBufferMaxEntries   = 0;
    uint32_t                     myViewLayerMask          = 0xFFFFFFFFu;
    bool                         myGpuCullEnabled         = false;
    const Vk_ResourceTables*     myResourceTables         = nullptr;
};

class Vk_FrameDrawPrep {
public:
    size_t                myDrawCountBeforeCull = 0;
    Gfx_FrameRenderPacket myFramePacket;

    Gfx_FrameDrawStreamLogState myStreamLogs;

    void ClearFrameOutputs();
    void ResetLogState();

    // Returns false when instance slab overflow (record should be skipped).
    bool Build( const Vk_FrameDrawPrepBuildParams& aParams );

    // Scene-wide SoA → entity-record SSBO (once per frame, before multi-view Build).
    bool FillEntityRecords( const Gfx_SceneSoA& aScene, const Vk_ResourceTables& aTables, uint32_t aCurrentFrame, std::vector< Vk_FrameData >& aFrameDatas );

private:
    bool FillInstanceSlab( const Vk_FrameDrawPrepBuildParams& aParams, Gfx_FrameRenderPacket& aPacket );
    bool FillDrawTemplates( const Vk_FrameDrawPrepBuildParams& aParams, Gfx_FrameRenderPacket& aPacket );

    bool mySlabFillLoggedOnce         = false;
    bool myInstanceSlabOverflowLogged = false;
    bool myDrawTemplateFillLoggedOnce = false;
    bool myDrawTemplateOverflowLogged = false;
    bool myEntityRecordFillLoggedOnce = false;
    bool myEntityRecordOverflowLogged = false;
};

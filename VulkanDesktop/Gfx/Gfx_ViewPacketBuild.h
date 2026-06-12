#pragma once

#include <glm/glm.hpp>

#include "Gfx_FrameDrawStream.h"
#include "Gfx_RenderPacket.h"

struct Gfx_ViewPacketBuildParams {
    Gfx_SceneSoA*       myScene    = nullptr;
    const Gfx_LodTable* myLodTable = nullptr;
    Gfx_LodState*       myLodState = nullptr;
    glm::mat4           myView{ 1.0f };
    glm::mat4           myProj{ 1.0f };
    glm::vec3           myCameraEye{};
    uint32_t            myViewLayerMask         = 0xFFFFFFFFu;
    bool                myLodEnabled            = false;
    uint32_t            myLodDebugLogicalMeshId = UINT32_MAX;
    bool                myGpuCullEnabled        = false;
};

// Gfx-owned extract → cull → LOD → sort → batch (no Vulkan, no instance slab).
void Gfx_BuildViewFramePacket( const Gfx_ViewPacketBuildParams& aParams, Gfx_FrameDrawStreamLogState& aLogs, Gfx_FrameRenderPacket& aOutPacket,
                               size_t& aOutDrawCountBeforeCull );

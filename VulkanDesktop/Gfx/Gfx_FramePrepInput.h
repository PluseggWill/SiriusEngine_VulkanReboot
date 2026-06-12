#pragma once

#include <cstdint>

#include "Gfx_Lod.h"
#include "Gfx_SceneSoA.h"

// CPU scene columns RenderCore reads during frame prep — no App/WorldState in RenderCore.
struct Gfx_FramePrepInput {
    Gfx_SceneSoA* myScene                 = nullptr;
    Gfx_LodTable* myLodTable              = nullptr;
    Gfx_LodState* myLodState              = nullptr;
    uint32_t      myLodDebugLogicalMeshId = UINT32_MAX;
};

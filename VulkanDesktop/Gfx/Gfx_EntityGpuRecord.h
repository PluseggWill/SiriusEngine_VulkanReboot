#pragma once

#include "../RenderContract/Gpu_EntityRecord.h"
#include "Gfx_Lod.h"
#include "Gfx_SceneSoA.h"

#include <glm/glm.hpp>

// Optional LOD: when myLodEnabled, resolve physical mesh id from myLodTable (primary-view eye). myLodState is snapshotted by caller.
struct Gfx_EntityRecordLodParams {
    bool                myLodEnabled = false;
    glm::vec3           myCameraEye{};
    const Gfx_LodTable* myLodTable = nullptr;
    Gfx_LodState*       myLodState = nullptr;
};

void Gfx_FillEntityGpuRecord( Gpu_EntityRecord& aOut, const Gfx_SceneSoA& aScene, uint32_t aSlot, uint32_t aIndexCount );

uint32_t Gfx_ResolveEntityRecordMeshId( const Gfx_SceneSoA& aScene, uint32_t aSlot, const Gfx_EntityRecordLodParams& aLod );

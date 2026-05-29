#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_DrawExtract.h"
#include "Gfx_SceneSoA.h"

// CPU LOD v0: logical mesh id (SoA) → resolved physical mesh id (draw instance). No Vulkan.

struct Gfx_LodChain {
    std::vector< uint32_t > myMeshIds;
    std::vector< float >    myDistanceThresholds;
};

class Gfx_LodTable {
public:
    void SetChain( uint32_t aLogicalMeshId, Gfx_LodChain aChain );

    const Gfx_LodChain* GetChain( uint32_t aLogicalMeshId ) const;
    uint32_t            GetLodCount( uint32_t aLogicalMeshId ) const;
    uint32_t            GetResolvedMeshId( uint32_t aLogicalMeshId, uint32_t aLodLevel ) const;

private:
    std::vector< Gfx_LodChain > myChains;
};

// Per-entity-slot hysteresis state (size grows with SoA slots).
class Gfx_LodState {
public:
    void EnsureSlotCount( uint32_t aSlotCount );
    void Clear();

    uint32_t GetPreviousLod( uint32_t aSlot ) const;
    void     SetPreviousLod( uint32_t aSlot, uint32_t aLodLevel );

private:
    std::vector< uint32_t > myPreviousLodBySlot;
};

uint32_t Gfx_SelectLodLevel( float aEyeDistance, float aLodBias, const Gfx_LodChain& aChain );

uint32_t Gfx_ApplyLodHysteresis( uint32_t aSlot, uint32_t aCandidateLod, const Gfx_LodChain& aChain, float aEyeDistance, float aLodBias, Gfx_LodState& aState );

// After cull: resolve mesh ids on draw instances; refresh opaque sort keys. Transparent sort keys unchanged (mesh not in key).
void Gfx_ApplyLodToFrameExtract( const Gfx_SceneSoA& aScene, const glm::vec3& aEyeWorld, const Gfx_LodTable& aTable, Gfx_LodState& aState,
                                 Gfx_FrameExtract& aInOut );

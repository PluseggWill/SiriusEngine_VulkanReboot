#include "Gfx_Lod.h"

#include "../Util/Util_DemoAssets.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr uint32_t kLodUnset     = UINT32_MAX;
constexpr float    kHysteresisUp = 0.85f;
constexpr float    kHysteresisDn = 1.15f;

glm::vec3 BoundsCenter( const Gfx_Bounds& aBounds ) {
    return ( aBounds.myMin + aBounds.myMax ) * 0.5f;
}

float EyeDistance( const glm::vec3& aEyeWorld, const glm::vec3& aWorldPoint ) {
    return glm::length( aWorldPoint - aEyeWorld );
}

void ApplyLodToResult( const Gfx_SceneSoA& aScene, const glm::vec3& aEyeWorld, const Gfx_LodTable& aTable, Gfx_LodState& aState, Gfx_ExtractResult& aInOut ) {
    for ( Gfx_DrawInstance& draw : aInOut.myDrawInstances ) {
        const uint32_t slot       = draw.myEntityIndex;
        const uint32_t logicalId  = aScene.GetLogicalMeshId( slot );
        const float    lodBias    = aScene.GetLodBias( slot );
        const Gfx_LodChain* chain = aTable.GetChain( logicalId );
        if ( chain == nullptr || chain->myMeshIds.empty() ) {
            continue;
        }

        const float eyeDist     = EyeDistance( aEyeWorld, BoundsCenter( aScene.GetBounds( slot ) ) );
        const uint32_t candidate = Gfx_SelectLodLevel( eyeDist, lodBias, *chain );
        const uint32_t lodLevel  = Gfx_ApplyLodHysteresis( slot, candidate, *chain, eyeDist, lodBias, aState );
        const uint32_t resolved  = aTable.GetResolvedMeshId( logicalId, lodLevel );

        draw.myMeshId = resolved;

        const uint16_t depthBucket = static_cast< uint16_t >( draw.mySortKey & 0xFFFFu );
        draw.mySortKey             = Gfx_PackOpaqueSortKey( draw.myPipelinePermutationId, draw.myMaterialId, resolved, depthBucket );
    }
}
}  // namespace

void Gfx_LodTable::SetChain( uint32_t aLogicalMeshId, Gfx_LodChain aChain ) {
    if ( aLogicalMeshId >= myChains.size() ) {
        myChains.resize( aLogicalMeshId + 1 );
    }
    myChains[ aLogicalMeshId ] = std::move( aChain );
}

const Gfx_LodChain* Gfx_LodTable::GetChain( uint32_t aLogicalMeshId ) const {
    if ( aLogicalMeshId >= myChains.size() || myChains[ aLogicalMeshId ].myMeshIds.empty() ) {
        return nullptr;
    }
    return &myChains[ aLogicalMeshId ];
}

uint32_t Gfx_LodTable::GetLodCount( uint32_t aLogicalMeshId ) const {
    const Gfx_LodChain* chain = GetChain( aLogicalMeshId );
    return chain != nullptr ? static_cast< uint32_t >( chain->myMeshIds.size() ) : 1u;
}

uint32_t Gfx_LodTable::GetResolvedMeshId( uint32_t aLogicalMeshId, uint32_t aLodLevel ) const {
    const Gfx_LodChain* chain = GetChain( aLogicalMeshId );
    if ( chain == nullptr || chain->myMeshIds.empty() ) {
        return aLogicalMeshId;
    }
    const uint32_t clamped = std::min( aLodLevel, static_cast< uint32_t >( chain->myMeshIds.size() ) - 1u );
    return chain->myMeshIds[ clamped ];
}

void Gfx_LodState::EnsureSlotCount( uint32_t aSlotCount ) {
    if ( myPreviousLodBySlot.size() < aSlotCount ) {
        myPreviousLodBySlot.resize( aSlotCount, kLodUnset );
    }
}

void Gfx_LodState::Clear() {
    myPreviousLodBySlot.clear();
}

uint32_t Gfx_LodState::GetPreviousLod( uint32_t aSlot ) const {
    if ( aSlot >= myPreviousLodBySlot.size() ) {
        return kLodUnset;
    }
    return myPreviousLodBySlot[ aSlot ];
}

void Gfx_LodState::SetPreviousLod( uint32_t aSlot, uint32_t aLodLevel ) {
    EnsureSlotCount( aSlot + 1 );
    myPreviousLodBySlot[ aSlot ] = aLodLevel;
}

void Gfx_BuildDemoLodTable( Gfx_LodTable& aOut ) {
    aOut = Gfx_LodTable{};

    auto singleMesh = []( uint32_t aMeshId ) {
        Gfx_LodChain chain{};
        chain.myMeshIds.push_back( aMeshId );
        return chain;
    };

    aOut.SetChain( UtilDemoAssets::kLogicalViking, singleMesh( UtilDemoAssets::kMeshViking ) );
    aOut.SetChain( UtilDemoAssets::kLogicalMonkey, singleMesh( UtilDemoAssets::kMeshMonkey ) );
    aOut.SetChain( UtilDemoAssets::kLogicalRock, singleMesh( UtilDemoAssets::kMeshRock ) );
    aOut.SetChain( UtilDemoAssets::kLogicalCampfire, singleMesh( UtilDemoAssets::kMeshCampfire ) );
    aOut.SetChain( UtilDemoAssets::kLogicalTent, singleMesh( UtilDemoAssets::kMeshTent ) );
    aOut.SetChain( UtilDemoAssets::kLogicalStump, singleMesh( UtilDemoAssets::kMeshStump ) );

    Gfx_LodChain tree{};
    tree.myMeshIds             = { UtilDemoAssets::kMeshTreeDetailed, UtilDemoAssets::kMeshTreeSimple };
    tree.myDistanceThresholds  = { 14.0f };
    aOut.SetChain( UtilDemoAssets::kLogicalTree, std::move( tree ) );
}

uint32_t Gfx_SelectLodLevel( float aEyeDistance, float aLodBias, const Gfx_LodChain& aChain ) {
    const float adjustedDistance = std::max( 0.0f, aEyeDistance + aLodBias );
    uint32_t    lod              = 0;
    for ( const float threshold : aChain.myDistanceThresholds ) {
        if ( adjustedDistance >= threshold ) {
            ++lod;
        }
        else {
            break;
        }
    }
    return std::min( lod, static_cast< uint32_t >( aChain.myMeshIds.size() ) - 1u );
}

uint32_t Gfx_ApplyLodHysteresis( uint32_t aSlot, uint32_t aCandidateLod, const Gfx_LodChain& aChain, float aEyeDistance, float aLodBias, Gfx_LodState& aState ) {
    aState.EnsureSlotCount( aSlot + 1 );
    const uint32_t previous = aState.GetPreviousLod( aSlot );
    if ( previous == kLodUnset ) {
        aState.SetPreviousLod( aSlot, aCandidateLod );
        return aCandidateLod;
    }

    const float adjustedDistance = std::max( 0.0f, aEyeDistance + aLodBias );
    uint32_t    lod              = previous;

    if ( aCandidateLod > previous ) {
        for ( uint32_t level = previous; level < aCandidateLod && level < aChain.myDistanceThresholds.size(); ++level ) {
            const float threshold = aChain.myDistanceThresholds[ level ] * kHysteresisDn;
            if ( adjustedDistance >= threshold ) {
                lod = level + 1;
            }
            else {
                break;
            }
        }
    }
    else if ( aCandidateLod < previous ) {
        for ( int level = static_cast< int >( previous ); level > static_cast< int >( aCandidateLod ); --level ) {
            const uint32_t thresholdIndex = static_cast< uint32_t >( level ) - 1u;
            if ( thresholdIndex >= aChain.myDistanceThresholds.size() ) {
                continue;
            }
            const float threshold = aChain.myDistanceThresholds[ thresholdIndex ] * kHysteresisUp;
            if ( adjustedDistance < threshold ) {
                lod = static_cast< uint32_t >( level ) - 1u;
            }
            else {
                break;
            }
        }
    }

    aState.SetPreviousLod( aSlot, lod );
    return lod;
}

void Gfx_ApplyLodToFrameExtract( const Gfx_SceneSoA& aScene, const glm::vec3& aEyeWorld, const Gfx_LodTable& aTable, Gfx_LodState& aState,
                                 Gfx_FrameExtract& aInOut ) {
    aState.EnsureSlotCount( aScene.GetSlotCount() );
    ApplyLodToResult( aScene, aEyeWorld, aTable, aState, aInOut.myOpaque );
    ApplyLodToResult( aScene, aEyeWorld, aTable, aState, aInOut.myTransparent );
}

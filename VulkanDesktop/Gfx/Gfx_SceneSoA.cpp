#include "Gfx_SceneSoA.h"

#include "Gfx_Bounds.h"

#include <algorithm>

namespace {
constexpr uint32_t kInvalidSlot = UINT32_MAX;

Gfx_Bounds DefaultUnitLocalBounds() {
    Gfx_Bounds bounds{};
    bounds.myMin = glm::vec3( -0.5f );
    bounds.myMax = glm::vec3( 0.5f );
    return bounds;
}
}  // namespace

void Gfx_SceneSoA::Clear() {
    myTransforms.clear();
    myLocalBounds.clear();
    myBounds.clear();
    myLogicalMeshIds.clear();
    myLodBiases.clear();
    myMaterialIds.clear();
    myLayerMasks.clear();
    myRenderFlags.clear();
    myGenerations.clear();
    myActiveSlots.clear();
    myFreeSlots.clear();
}

void Gfx_SceneSoA::GrowOneSlot() {
    myTransforms.emplace_back( 1.0f );
    myLocalBounds.push_back( DefaultUnitLocalBounds() );
    myBounds.emplace_back();
    myLogicalMeshIds.push_back( 0 );
    myLodBiases.push_back( 0.0f );
    myMaterialIds.push_back( 0 );
    myLayerMasks.push_back( 0xFFFFFFFFu );
    myRenderFlags.push_back( Gfx_RenderOpaque );
    myGenerations.push_back( 0 );
}

void Gfx_SceneSoA::UpdateBoundsForSlot( uint32_t aSlot ) {
    myBounds[ aSlot ] = Gfx_TransformBounds( myLocalBounds[ aSlot ], myTransforms[ aSlot ] );
}

Gfx_StableEntityId Gfx_SceneSoA::AllocEntity( uint32_t aLogicalMeshId, uint32_t aMaterialId, const glm::mat4& aWorldTransform, uint32_t aLayerMask,
                                              Gfx_RenderFlags aRenderFlags, float aLodBias ) {
    uint32_t slot = kInvalidSlot;
    if ( !myFreeSlots.empty() ) {
        slot = myFreeSlots.back();
        myFreeSlots.pop_back();
    }
    else {
        slot = static_cast< uint32_t >( myTransforms.size() );
        GrowOneSlot();
    }

    myLogicalMeshIds[ slot ] = aLogicalMeshId;
    myLodBiases[ slot ]      = aLodBias;
    myMaterialIds[ slot ]    = aMaterialId;
    myLayerMasks[ slot ]     = aLayerMask;
    myRenderFlags[ slot ]    = aRenderFlags;
    myTransforms[ slot ]     = aWorldTransform;
    myLocalBounds[ slot ]    = DefaultUnitLocalBounds();
    UpdateBoundsForSlot( slot );

    myActiveSlots.push_back( slot );

    Gfx_StableEntityId id{};
    id.myIndex      = slot;
    id.myGeneration = myGenerations[ slot ];
    return id;
}

bool Gfx_SceneSoA::FreeEntity( Gfx_StableEntityId aId ) {
    if ( !IsAlive( aId ) ) {
        return false;
    }

    const uint32_t slot = aId.myIndex;
    myActiveSlots.erase( std::remove( myActiveSlots.begin(), myActiveSlots.end(), slot ), myActiveSlots.end() );

    ++myGenerations[ slot ];
    myFreeSlots.push_back( slot );
    return true;
}

bool Gfx_SceneSoA::IsAlive( Gfx_StableEntityId aId ) const {
    if ( aId.myIndex == kInvalidSlot || aId.myIndex >= myGenerations.size() ) {
        return false;
    }
    if ( aId.myGeneration != myGenerations[ aId.myIndex ] ) {
        return false;
    }
    return std::find( myActiveSlots.begin(), myActiveSlots.end(), aId.myIndex ) != myActiveSlots.end();
}

bool Gfx_SceneSoA::IsSlotActive( uint32_t aSlot ) const {
    return std::find( myActiveSlots.begin(), myActiveSlots.end(), aSlot ) != myActiveSlots.end();
}

const glm::mat4& Gfx_SceneSoA::GetWorldTransform( uint32_t aSlot ) const {
    return myTransforms.at( aSlot );
}

const Gfx_Bounds& Gfx_SceneSoA::GetBounds( uint32_t aSlot ) const {
    return myBounds.at( aSlot );
}

uint32_t Gfx_SceneSoA::GetLogicalMeshId( uint32_t aSlot ) const {
    return myLogicalMeshIds.at( aSlot );
}

float Gfx_SceneSoA::GetLodBias( uint32_t aSlot ) const {
    return myLodBiases.at( aSlot );
}

uint32_t Gfx_SceneSoA::GetMaterialId( uint32_t aSlot ) const {
    return myMaterialIds.at( aSlot );
}

uint32_t Gfx_SceneSoA::GetLayerMask( uint32_t aSlot ) const {
    return myLayerMasks.at( aSlot );
}

Gfx_RenderFlags Gfx_SceneSoA::GetRenderFlags( uint32_t aSlot ) const {
    return static_cast< Gfx_RenderFlags >( myRenderFlags.at( aSlot ) );
}

uint32_t Gfx_SceneSoA::GetGeneration( uint32_t aSlot ) const {
    return myGenerations.at( aSlot );
}

void Gfx_SceneSoA::SetWorldTransform( uint32_t aSlot, const glm::mat4& aWorldTransform ) {
    myTransforms.at( aSlot ) = aWorldTransform;
    UpdateBoundsForSlot( aSlot );
}

void Gfx_SceneSoA::SetLocalBoundsForSlot( uint32_t aSlot, const Gfx_Bounds& aLocalBounds ) {
    myLocalBounds.at( aSlot ) = aLocalBounds;
    UpdateBoundsForSlot( aSlot );
}

#include "Gfx_SceneSoA.h"

#include <algorithm>

namespace {
constexpr uint32_t kInvalidSlot = UINT32_MAX;
}

void Gfx_SceneSoA::Clear() {
    myTransforms.clear();
    myBounds.clear();
    myMeshIds.clear();
    myMaterialIds.clear();
    myLayerMasks.clear();
    myRenderFlags.clear();
    myGenerations.clear();
    myActiveSlots.clear();
    myFreeSlots.clear();
}

void Gfx_SceneSoA::GrowOneSlot() {
    myTransforms.emplace_back( 1.0f );
    myBounds.emplace_back();
    myMeshIds.push_back( 0 );
    myMaterialIds.push_back( 0 );
    myLayerMasks.push_back( 0xFFFFFFFFu );
    myRenderFlags.push_back( Gfx_RenderOpaque );
    myGenerations.push_back( 0 );
}

void Gfx_SceneSoA::UpdateBoundsForSlot( uint32_t aSlot ) {
    const glm::mat4& m      = myTransforms[ aSlot ];
    const glm::vec3  center = glm::vec3( m[ 3 ] );
    glm::vec3        halfExtents{
        std::max( glm::length( glm::vec3( m[ 0 ] ) ), 0.01f ),
        std::max( glm::length( glm::vec3( m[ 1 ] ) ), 0.01f ),
        std::max( glm::length( glm::vec3( m[ 2 ] ) ), 0.01f ),
    };

    Gfx_Bounds bounds{};
    bounds.myMin = center - halfExtents;
    bounds.myMax = center + halfExtents;
    myBounds[ aSlot ] = bounds;
}

Gfx_StableEntityId Gfx_SceneSoA::AllocEntity( uint32_t aMeshId, uint32_t aMaterialId, const glm::mat4& aWorldTransform, uint32_t aLayerMask,
                                            Gfx_RenderFlags aRenderFlags ) {
    uint32_t slot = kInvalidSlot;
    if ( !myFreeSlots.empty() ) {
        slot = myFreeSlots.back();
        myFreeSlots.pop_back();
    }
    else {
        slot = static_cast< uint32_t >( myTransforms.size() );
        GrowOneSlot();
    }

    myMeshIds[ slot ]       = aMeshId;
    myMaterialIds[ slot ]   = aMaterialId;
    myLayerMasks[ slot ]    = aLayerMask;
    myRenderFlags[ slot ]   = aRenderFlags;
    myTransforms[ slot ]    = aWorldTransform;
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

const glm::mat4& Gfx_SceneSoA::GetTransform( uint32_t aSlot ) const {
    return myTransforms.at( aSlot );
}

const Gfx_Bounds& Gfx_SceneSoA::GetBounds( uint32_t aSlot ) const {
    return myBounds.at( aSlot );
}

uint32_t Gfx_SceneSoA::GetMeshId( uint32_t aSlot ) const {
    return myMeshIds.at( aSlot );
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

void Gfx_SceneSoA::SetTransform( uint32_t aSlot, const glm::mat4& aWorldTransform ) {
    myTransforms.at( aSlot ) = aWorldTransform;
    UpdateBoundsForSlot( aSlot );
}

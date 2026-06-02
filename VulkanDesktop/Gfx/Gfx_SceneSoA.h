#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// Columnar scene store (S1 data plane). No Vulkan.

struct Gfx_StableEntityId {
    uint32_t myIndex      = UINT32_MAX;
    uint32_t myGeneration = 0;
};

struct Gfx_Bounds {
    glm::vec3 myMin{ 0.0f };
    glm::vec3 myMax{ 0.0f };
};

enum Gfx_RenderFlags : uint32_t {
    Gfx_RenderOpaque      = 0,
    Gfx_RenderTransparent = 1,
};

class Gfx_SceneSoA {
public:
    void Clear();

    Gfx_StableEntityId AllocEntity( uint32_t aLogicalMeshId, uint32_t aMaterialId, const glm::mat4& aWorldTransform, uint32_t aLayerMask = 0xFFFFFFFFu,
                                    Gfx_RenderFlags aRenderFlags = Gfx_RenderOpaque, float aLodBias = 0.0f );
    bool               FreeEntity( Gfx_StableEntityId aId );
    bool               IsAlive( Gfx_StableEntityId aId ) const;

    uint32_t GetActiveCount() const {
        return static_cast< uint32_t >( myActiveSlots.size() );
    }
    uint32_t GetSlotCount() const {
        return static_cast< uint32_t >( myTransforms.size() );
    }
    bool IsSlotActive( uint32_t aSlot ) const;

    const std::vector< uint32_t >& GetActiveSlots() const {
        return myActiveSlots;
    }

    const glm::mat4&  GetWorldTransform( uint32_t aSlot ) const;
    const Gfx_Bounds& GetBounds( uint32_t aSlot ) const;
    uint32_t          GetLogicalMeshId( uint32_t aSlot ) const;
    float             GetLodBias( uint32_t aSlot ) const;
    uint32_t          GetMaterialId( uint32_t aSlot ) const;
    uint32_t          GetLayerMask( uint32_t aSlot ) const;
    Gfx_RenderFlags   GetRenderFlags( uint32_t aSlot ) const;
    uint32_t          GetGeneration( uint32_t aSlot ) const;

    void SetWorldTransform( uint32_t aSlot, const glm::mat4& aWorldTransform );

private:
    void GrowOneSlot();
    void UpdateBoundsForSlot( uint32_t aSlot );

    std::vector< glm::mat4 >  myTransforms;
    std::vector< Gfx_Bounds > myBounds;
    std::vector< uint32_t >   myLogicalMeshIds;
    std::vector< float >      myLodBiases;
    std::vector< uint32_t >   myMaterialIds;
    std::vector< uint32_t >   myLayerMasks;
    std::vector< uint32_t >   myRenderFlags;
    std::vector< uint32_t >   myGenerations;
    std::vector< uint32_t >   myActiveSlots;
    std::vector< uint32_t >   myFreeSlots;
};

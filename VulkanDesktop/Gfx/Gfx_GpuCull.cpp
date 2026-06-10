// Module: Gfx_GpuCull — CPU reference for EntityCull.comp (G1 parity vs CPU draw-stream cull).
#include "Gfx_GpuCull.h"

#include "Gfx_DrawCullSort.h"
#include "Gfx_DrawExtract.h"
#include "Gfx_SceneSoA.h"

#include <algorithm>

namespace {

bool IsEntitySlotVisibleForGpuCull( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, uint32_t aSlot, const Gfx_FrustumPlanes& aFrustum ) {
    // Inactive slots upload layerMask == 0 in entity-record SSBO; shader rejects before frustum test.
    if ( !aScene.IsSlotActive( aSlot ) ) {
        return false;
    }

    const uint32_t layerMask = aScene.GetLayerMask( aSlot );
    if ( layerMask == 0u || ( layerMask & aView.myViewLayerMask ) == 0u ) {
        return false;
    }

    return Gfx_IsBoundsVisible( aScene.GetBounds( aSlot ), aFrustum );
}

void AppendCullVisibleEntitySlots( const Gfx_ExtractResult& aPass, std::vector< uint32_t >& aOutSlots ) {
    for ( const Gfx_DrawInstance& draw : aPass.myDrawInstances ) {
        aOutSlots.push_back( draw.myEntityIndex );
    }
}

void SortUniqueInPlace( std::vector< uint32_t >& aSlots ) {
    std::sort( aSlots.begin(), aSlots.end() );
    aSlots.erase( std::unique( aSlots.begin(), aSlots.end() ), aSlots.end() );
}

}  // namespace

bool Gfx_IsEntitySlotVisibleForGpuCull( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, uint32_t aSlot ) {
    const Gfx_FrustumPlanes frustum = Gfx_BuildFrustumFromViewProj( aView.myProj * aView.myView );
    return IsEntitySlotVisibleForGpuCull( aScene, aView, aSlot, frustum );
}

void Gfx_CollectCpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots ) {
    aOutSlots.clear();

    Gfx_FrameExtract extract{};
    Gfx_ExtractDrawInstances( aScene, aView, extract );
    Gfx_CullDrawInstancesInPlace( aScene, aView, extract.myOpaque );
    Gfx_CullDrawInstancesInPlace( aScene, aView, extract.myTransparent );

    aOutSlots.reserve( extract.myOpaque.myDrawInstances.size() + extract.myTransparent.myDrawInstances.size() );
    AppendCullVisibleEntitySlots( extract.myOpaque, aOutSlots );
    AppendCullVisibleEntitySlots( extract.myTransparent, aOutSlots );
    SortUniqueInPlace( aOutSlots );
}

void Gfx_CollectGpuCullVisibleSlots( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, std::vector< uint32_t >& aOutSlots ) {
    aOutSlots.clear();

    const Gfx_FrustumPlanes frustum   = Gfx_BuildFrustumFromViewProj( aView.myProj * aView.myView );
    const uint32_t          slotCount = aScene.GetSlotCount();
    for ( uint32_t slot = 0; slot < slotCount; ++slot ) {
        if ( IsEntitySlotVisibleForGpuCull( aScene, aView, slot, frustum ) ) {
            aOutSlots.push_back( slot );
        }
    }
}

bool Gfx_AreCpuGpuCullVisibleSlotsEqual( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView ) {
    std::vector< uint32_t > cpuSlots;
    std::vector< uint32_t > gpuSlots;
    Gfx_CollectCpuCullVisibleSlots( aScene, aView, cpuSlots );
    Gfx_CollectGpuCullVisibleSlots( aScene, aView, gpuSlots );
    return cpuSlots == gpuSlots;
}

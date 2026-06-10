#include "Gfx_EntityGpuRecord.h"

void Gfx_FillEntityGpuRecord( Gfx_EntityGpuRecord& aOut, const Gfx_SceneSoA& aScene, uint32_t aSlot, uint32_t aIndexCount ) {
    aOut = Gfx_EntityGpuRecord{};

    if ( !aScene.IsSlotActive( aSlot ) ) {
        return;
    }

    const Gfx_Bounds& bounds      = aScene.GetBounds( aSlot );
    aOut.myBoundsMin              = glm::vec4( bounds.myMin, 0.0f );
    aOut.myBoundsMax              = glm::vec4( bounds.myMax, 0.0f );
    aOut.myLayerMask              = aScene.GetLayerMask( aSlot );
    aOut.myRenderFlags            = static_cast< uint32_t >( aScene.GetRenderFlags( aSlot ) );
    aOut.myLogicalMeshId          = aScene.GetLogicalMeshId( aSlot );
    aOut.myMaterialId             = aScene.GetMaterialId( aSlot );
    aOut.myIndirect.indexCount    = aIndexCount;
    aOut.myIndirect.instanceCount = 1;
}

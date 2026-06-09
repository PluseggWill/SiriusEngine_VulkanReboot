#include "Gfx_DrawExtract.h"

#include "Gfx_Bounds.h"
#include "Gfx_ShaderPermutation.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Global for v0 demo sort-key table generation (scene-load will pass view/resource context).
static uint16_t gMaterialTableGeneration = 0;

void Gfx_SetMaterialTableGenerationForExtract( uint16_t aGeneration ) {
    gMaterialTableGeneration = aGeneration;
}

namespace {
constexpr float kDepthBucketScale = 256.0f;

void AppendDraw( Gfx_ExtractResult& aOut, uint32_t aSlot, const Gfx_SceneSoA& aScene, uint16_t aDepthBucket ) {
    const uint32_t meshId     = aScene.GetLogicalMeshId( aSlot );
    const uint32_t materialId = aScene.GetMaterialId( aSlot );
    // GfxTests and other CPU-only callers may run extract without full engine init.
    const uint16_t shaderPerm = Gfx_ShaderPermutation::IsInitialized() ? static_cast< uint16_t >( Gfx_ShaderPermutation::GetActiveId() ) : 0;
    const uint16_t permSlot   = Gfx_ShaderPermutation::EncodeSortKeyPermSlot( shaderPerm, gMaterialTableGeneration );
    const uint64_t sortKey    = Gfx_PackOpaqueSortKey( permSlot, materialId, meshId, aDepthBucket );

    Gfx_DrawInstance draw{};
    draw.mySortKey               = sortKey;
    draw.myMeshId                = meshId;
    draw.myMaterialId            = materialId;
    draw.myInstanceDataOffset    = 0;
    draw.myPipelinePermutationId = shaderPerm;
    draw.myEntityIndex           = aSlot;

    aOut.myVisibleEntityIndices.push_back( aSlot );
    aOut.myDrawInstances.push_back( draw );
}
}  // namespace

uint64_t Gfx_PackOpaqueSortKey( uint32_t aPermSlot, uint32_t aMaterialId, uint32_t aMeshId, uint16_t aDepthBucket ) {
    const uint64_t perm  = static_cast< uint64_t >( aPermSlot & 0xFFFFu ) << 48;
    const uint64_t mat   = static_cast< uint64_t >( aMaterialId & 0xFFFFu ) << 32;
    const uint64_t mesh  = static_cast< uint64_t >( aMeshId & 0xFFFFu ) << 16;
    const uint64_t depth = static_cast< uint64_t >( aDepthBucket );
    return perm | mat | mesh | depth;
}

uint16_t Gfx_ComputeDepthBucket( float aEyeSpaceZ ) {
    const float clamped = std::clamp( aEyeSpaceZ, -128.0f, 127.0f );
    const int   bucket  = static_cast< int >( std::floor( clamped * kDepthBucketScale ) );
    return static_cast< uint16_t >( std::clamp( bucket, 0, static_cast< int >( std::numeric_limits< uint16_t >::max() ) ) );
}

float Gfx_ComputeEyeSpaceZ( const glm::mat4& aView, const glm::vec3& aWorldPosition ) {
    const glm::vec4 eye = aView * glm::vec4( aWorldPosition, 1.0f );
    return eye.z;
}

void Gfx_ExtractDrawInstances( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, Gfx_FrameExtract& aOut ) {
    aOut.myOpaque.myVisibleEntityIndices.clear();
    aOut.myOpaque.myDrawInstances.clear();
    aOut.myTransparent.myVisibleEntityIndices.clear();
    aOut.myTransparent.myDrawInstances.clear();

    const std::vector< uint32_t >& activeSlots = aScene.GetActiveSlots();
    aOut.myOpaque.myVisibleEntityIndices.reserve( activeSlots.size() );
    aOut.myOpaque.myDrawInstances.reserve( activeSlots.size() );
    aOut.myTransparent.myVisibleEntityIndices.reserve( activeSlots.size() );
    aOut.myTransparent.myDrawInstances.reserve( activeSlots.size() );

    for ( const uint32_t slot : activeSlots ) {
        const Gfx_Bounds& bounds      = aScene.GetBounds( slot );
        const float       eyeSpaceZ   = Gfx_ComputeEyeSpaceZ( aView.myView, Gfx_BoundsCenter( bounds ) );
        const uint16_t    depthBucket = Gfx_ComputeDepthBucket( eyeSpaceZ );
        if ( aScene.GetRenderFlags( slot ) == Gfx_RenderTransparent ) {
            AppendDraw( aOut.myTransparent, slot, aScene, depthBucket );
        }
        else {
            AppendDraw( aOut.myOpaque, slot, aScene, depthBucket );
        }
    }
}

#include "Gfx_DrawExtract.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kDepthBucketScale = 256.0f;

void AppendDraw( Gfx_ExtractResult& aOut, uint32_t aSlot, const Gfx_SceneSoA& aScene, uint16_t aDepthBucket ) {
    const uint32_t meshId     = aScene.GetLogicalMeshId( aSlot );
    const uint32_t materialId = aScene.GetMaterialId( aSlot );
    const uint64_t sortKey    = Gfx_PackOpaqueSortKey( 0, materialId, meshId, aDepthBucket );

    Gfx_DrawInstance draw{};
    draw.mySortKey               = sortKey;
    draw.myMeshId                = meshId;
    draw.myMaterialId            = materialId;
    draw.myInstanceDataOffset    = 0;
    draw.myPipelinePermutationId = 0;
    draw.myEntityIndex           = aSlot;

    aOut.myVisibleEntityIndices.push_back( aSlot );
    aOut.myDrawInstances.push_back( draw );
}
}  // namespace

uint64_t Gfx_PackOpaqueSortKey( uint32_t aPipelinePermutationId, uint32_t aMaterialId, uint32_t aMeshId, uint16_t aDepthBucket ) {
    const uint64_t perm  = static_cast< uint64_t >( aPipelinePermutationId & 0xFFFFu ) << 48;
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

    const glm::mat4 viewProj = aView.myProj * aView.myView;

    for ( const uint32_t slot : activeSlots ) {
        const glm::mat4& transform   = aScene.GetTransform( slot );
        const glm::vec3  worldOrigin = glm::vec3( transform[ 3 ] );
        const glm::vec4  clip        = viewProj * glm::vec4( worldOrigin, 1.0f );
        float            ndcZ        = 0.0f;
        if ( std::abs( clip.w ) > 1e-6f ) {
            const glm::vec4 ndc = clip / clip.w;
            ndcZ              = ndc.z;
        }

        const uint16_t depthBucket = Gfx_ComputeDepthBucket( ndcZ );
        if ( aScene.GetRenderFlags( slot ) == Gfx_RenderTransparent ) {
            AppendDraw( aOut.myTransparent, slot, aScene, depthBucket );
        }
        else {
            AppendDraw( aOut.myOpaque, slot, aScene, depthBucket );
        }
    }
}

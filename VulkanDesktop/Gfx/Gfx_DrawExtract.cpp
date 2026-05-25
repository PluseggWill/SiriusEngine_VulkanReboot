#include "Gfx_DrawExtract.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kDepthBucketScale = 256.0f;
}

uint64_t Gfx_PackOpaqueSortKey( uint32_t aPipelinePermutationId, uint32_t aMaterialId, uint32_t aMeshId, uint16_t aDepthBucket ) {
    const uint64_t perm   = static_cast< uint64_t >( aPipelinePermutationId & 0xFFFFu ) << 48;
    const uint64_t mat    = static_cast< uint64_t >( aMaterialId & 0xFFFFu ) << 32;
    const uint64_t mesh   = static_cast< uint64_t >( aMeshId & 0xFFFFu ) << 16;
    const uint64_t depth  = static_cast< uint64_t >( aDepthBucket );
    return perm | mat | mesh | depth;
}

uint16_t Gfx_ComputeDepthBucket( float aEyeSpaceZ ) {
    const float clamped = std::clamp( aEyeSpaceZ, -128.0f, 127.0f );
    const int   bucket  = static_cast< int >( std::floor( clamped * kDepthBucketScale ) );
    return static_cast< uint16_t >( std::clamp( bucket, 0, static_cast< int >( std::numeric_limits< uint16_t >::max() ) ) );
}

void Gfx_ExtractDrawInstances( const std::vector< Gfx_SceneEntity >& someEntities, const Gfx_ExtractViewParams& aView, Gfx_ExtractResult& aOut ) {
    aOut.myVisibleEntityIndices.clear();
    aOut.myDrawInstances.clear();
    aOut.myVisibleEntityIndices.reserve( someEntities.size() );
    aOut.myDrawInstances.reserve( someEntities.size() );

    const glm::mat4 viewProj = aView.myProj * aView.myView;

    for ( size_t entitySlot = 0; entitySlot < someEntities.size(); ++entitySlot ) {
        const Gfx_SceneEntity& entity = someEntities[ entitySlot ];

        const glm::vec4 worldOrigin = entity.myWorldTransform * glm::vec4( 0.0f, 0.0f, 0.0f, 1.0f );
        const glm::vec4 clip        = viewProj * worldOrigin;
        float           eyeZ        = 0.0f;
        if ( std::abs( clip.w ) > 1e-6f ) {
            const glm::vec4 ndc = clip / clip.w;
            eyeZ                = ndc.z;
        }

        const uint16_t depthBucket = Gfx_ComputeDepthBucket( eyeZ );
        const uint64_t sortKey     = Gfx_PackOpaqueSortKey( 0, entity.myMaterialId, entity.myMeshId, depthBucket );

        Gfx_DrawInstance draw{};
        draw.mySortKey               = sortKey;
        draw.myMeshId                = entity.myMeshId;
        draw.myMaterialId            = entity.myMaterialId;
        draw.myInstanceDataOffset    = static_cast< uint32_t >( entitySlot ) * sizeof( glm::mat4 );
        draw.myPipelinePermutationId = 0;
        draw.myEntityIndex           = entity.myId.myIndex;

        aOut.myVisibleEntityIndices.push_back( static_cast< uint32_t >( entitySlot ) );
        aOut.myDrawInstances.push_back( draw );
    }
}

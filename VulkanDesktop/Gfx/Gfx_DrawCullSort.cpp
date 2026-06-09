#include "Gfx_DrawCullSort.h"

#include "Gfx_Bounds.h"
#include "Gfx_DrawExtract.h"
#include "Gfx_SceneSoA.h"

#include <algorithm>
#include <numeric>

namespace {
void NormalizePlane( glm::vec4& aPlane ) {
    const float length = glm::length( glm::vec3( aPlane ) );
    if ( length > 1e-6f ) {
        aPlane /= length;
    }
}

bool IsAabbOutsidePlane( const Gfx_Bounds& aBounds, const glm::vec4& aPlane ) {
    const glm::vec3 p{ aPlane.x, aPlane.y, aPlane.z };
    const float     d = aPlane.w;

    const glm::vec3 positiveVertex{
        aPlane.x >= 0.0f ? aBounds.myMax.x : aBounds.myMin.x,
        aPlane.y >= 0.0f ? aBounds.myMax.y : aBounds.myMin.y,
        aPlane.z >= 0.0f ? aBounds.myMax.z : aBounds.myMin.z,
    };

    return glm::dot( p, positiveVertex ) + d < 0.0f;
}
}  // namespace

Gfx_FrustumPlanes Gfx_BuildFrustumFromViewProj( const glm::mat4& aViewProj ) {
    const glm::mat4 m = glm::transpose( aViewProj );

    Gfx_FrustumPlanes frustum{};
    frustum.myPlanes[ 0 ] = m[ 3 ] + m[ 0 ];
    frustum.myPlanes[ 1 ] = m[ 3 ] - m[ 0 ];
    frustum.myPlanes[ 2 ] = m[ 3 ] + m[ 1 ];
    frustum.myPlanes[ 3 ] = m[ 3 ] - m[ 1 ];
    frustum.myPlanes[ 4 ] = m[ 3 ] + m[ 2 ];
    frustum.myPlanes[ 5 ] = m[ 3 ] - m[ 2 ];

    for ( glm::vec4& plane : frustum.myPlanes ) {
        NormalizePlane( plane );
    }
    return frustum;
}

bool Gfx_IsBoundsVisible( const Gfx_Bounds& aBounds, const Gfx_FrustumPlanes& aFrustum ) {
    for ( const glm::vec4& plane : aFrustum.myPlanes ) {
        if ( IsAabbOutsidePlane( aBounds, plane ) ) {
            return false;
        }
    }
    return true;
}

void Gfx_CullDrawInstancesInPlace( const Gfx_SceneSoA& aScene, const Gfx_CullViewParams& aView, Gfx_ExtractResult& aInOut ) {
    const Gfx_FrustumPlanes frustum = Gfx_BuildFrustumFromViewProj( aView.myProj * aView.myView );

    const size_t inputCount = aInOut.myDrawInstances.size();
    size_t       writeIndex = 0;

    for ( size_t readIndex = 0; readIndex < inputCount; ++readIndex ) {
        const Gfx_DrawInstance& draw = aInOut.myDrawInstances[ readIndex ];
        const uint32_t          slot = draw.myEntityIndex;

        if ( ( aScene.GetLayerMask( slot ) & aView.myViewLayerMask ) == 0 ) {
            continue;
        }

        if ( !Gfx_IsBoundsVisible( aScene.GetBounds( slot ), frustum ) ) {
            continue;
        }

        if ( writeIndex != readIndex ) {
            aInOut.myDrawInstances[ writeIndex ]        = draw;
            aInOut.myVisibleEntityIndices[ writeIndex ] = aInOut.myVisibleEntityIndices[ readIndex ];
        }
        ++writeIndex;
    }

    aInOut.myDrawInstances.resize( writeIndex );
    aInOut.myVisibleEntityIndices.resize( writeIndex );
}

void Gfx_SortOpaqueDrawInstances( Gfx_ExtractResult& aResult ) {
    const size_t count = aResult.myDrawInstances.size();
    if ( count <= 1 ) {
        return;
    }

    std::vector< size_t > order( count );
    std::iota( order.begin(), order.end(), size_t{ 0 } );

    std::sort( order.begin(), order.end(),
               [ &aResult ]( size_t aLeft, size_t aRight ) { return aResult.myDrawInstances[ aLeft ].mySortKey < aResult.myDrawInstances[ aRight ].mySortKey; } );

    std::vector< Gfx_DrawInstance > sortedDraws;
    std::vector< uint32_t >         sortedVisible;
    sortedDraws.reserve( count );
    sortedVisible.reserve( count );

    for ( const size_t sourceIndex : order ) {
        sortedDraws.push_back( aResult.myDrawInstances[ sourceIndex ] );
        sortedVisible.push_back( aResult.myVisibleEntityIndices[ sourceIndex ] );
    }

    aResult.myDrawInstances        = std::move( sortedDraws );
    aResult.myVisibleEntityIndices = std::move( sortedVisible );
}

void Gfx_SortTransparentDrawInstances( Gfx_ExtractResult& aResult, const Gfx_SceneSoA& aScene, const glm::mat4& aView ) {
    const size_t count = aResult.myDrawInstances.size();
    if ( count <= 1 ) {
        return;
    }

    std::vector< size_t > order( count );
    std::iota( order.begin(), order.end(), size_t{ 0 } );

    auto eyeZForDraw = [ &aScene, &aView ]( const Gfx_DrawInstance& aDraw ) {
        return Gfx_ComputeEyeSpaceZ( aView, Gfx_BoundsCenter( aScene.GetBounds( aDraw.myEntityIndex ) ) );
    };

    std::sort( order.begin(), order.end(), [ &aResult, &eyeZForDraw ]( size_t aLeft, size_t aRight ) {
        const Gfx_DrawInstance& leftDraw  = aResult.myDrawInstances[ aLeft ];
        const Gfx_DrawInstance& rightDraw = aResult.myDrawInstances[ aRight ];

        const float eyeZLeft  = eyeZForDraw( leftDraw );
        const float eyeZRight = eyeZForDraw( rightDraw );
        if ( eyeZLeft != eyeZRight ) {
            return eyeZLeft < eyeZRight;
        }
        if ( leftDraw.myMaterialId != rightDraw.myMaterialId ) {
            return leftDraw.myMaterialId < rightDraw.myMaterialId;
        }
        return leftDraw.myEntityIndex < rightDraw.myEntityIndex;
    } );

    std::vector< Gfx_DrawInstance > sortedDraws;
    std::vector< uint32_t >         sortedVisible;
    sortedDraws.reserve( count );
    sortedVisible.reserve( count );

    for ( const size_t sourceIndex : order ) {
        sortedDraws.push_back( aResult.myDrawInstances[ sourceIndex ] );
        sortedVisible.push_back( aResult.myVisibleEntityIndices[ sourceIndex ] );
    }

    aResult.myDrawInstances        = std::move( sortedDraws );
    aResult.myVisibleEntityIndices = std::move( sortedVisible );
}

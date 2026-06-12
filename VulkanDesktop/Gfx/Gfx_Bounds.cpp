#include "Gfx_Bounds.h"

#include "Gfx_SceneSoA.h"

#include <algorithm>
#include <cmath>
#include <limits>

glm::vec3 Gfx_BoundsCenter( const Gfx_Bounds& aBounds ) {
    return ( aBounds.myMin + aBounds.myMax ) * 0.5f;
}

Gfx_Bounds Gfx_TransformBounds( const Gfx_Bounds& aLocalBounds, const glm::mat4& aWorldTransform ) {
    const glm::vec3 corners[ 8 ] = {
        { aLocalBounds.myMin.x, aLocalBounds.myMin.y, aLocalBounds.myMin.z }, { aLocalBounds.myMax.x, aLocalBounds.myMin.y, aLocalBounds.myMin.z },
        { aLocalBounds.myMin.x, aLocalBounds.myMax.y, aLocalBounds.myMin.z }, { aLocalBounds.myMax.x, aLocalBounds.myMax.y, aLocalBounds.myMin.z },
        { aLocalBounds.myMin.x, aLocalBounds.myMin.y, aLocalBounds.myMax.z }, { aLocalBounds.myMax.x, aLocalBounds.myMin.y, aLocalBounds.myMax.z },
        { aLocalBounds.myMin.x, aLocalBounds.myMax.y, aLocalBounds.myMax.z }, { aLocalBounds.myMax.x, aLocalBounds.myMax.y, aLocalBounds.myMax.z },
    };

    glm::vec3 worldMin = glm::vec3( aWorldTransform * glm::vec4( corners[ 0 ], 1.0f ) );
    glm::vec3 worldMax = worldMin;
    for ( int i = 1; i < 8; ++i ) {
        const glm::vec3 worldCorner = glm::vec3( aWorldTransform * glm::vec4( corners[ i ], 1.0f ) );
        worldMin                    = glm::min( worldMin, worldCorner );
        worldMax                    = glm::max( worldMax, worldCorner );
    }

    Gfx_Bounds worldBounds{};
    worldBounds.myMin = worldMin;
    worldBounds.myMax = worldMax;
    return worldBounds;
}

std::array< glm::vec3, 8 > Gfx_BoundsCorners( const Gfx_Bounds& aBounds ) {
    return {
        glm::vec3( aBounds.myMin.x, aBounds.myMin.y, aBounds.myMin.z ), glm::vec3( aBounds.myMax.x, aBounds.myMin.y, aBounds.myMin.z ),
        glm::vec3( aBounds.myMin.x, aBounds.myMax.y, aBounds.myMin.z ), glm::vec3( aBounds.myMax.x, aBounds.myMax.y, aBounds.myMin.z ),
        glm::vec3( aBounds.myMin.x, aBounds.myMin.y, aBounds.myMax.z ), glm::vec3( aBounds.myMax.x, aBounds.myMin.y, aBounds.myMax.z ),
        glm::vec3( aBounds.myMin.x, aBounds.myMax.y, aBounds.myMax.z ), glm::vec3( aBounds.myMax.x, aBounds.myMax.y, aBounds.myMax.z ),
    };
}

Gfx_Bounds Gfx_ComputeActiveOpaqueSceneBounds( const Gfx_SceneSoA& aScene ) {
    Gfx_Bounds merged{};
    merged.myMin = glm::vec3( std::numeric_limits< float >::max() );
    merged.myMax = glm::vec3( std::numeric_limits< float >::lowest() );

    for ( const uint32_t slot : aScene.GetActiveSlots() ) {
        if ( aScene.GetRenderFlags( slot ) != Gfx_RenderOpaque ) {
            continue;
        }
        const Gfx_Bounds& bounds = aScene.GetBounds( slot );
        merged.myMin             = glm::min( merged.myMin, bounds.myMin );
        merged.myMax             = glm::max( merged.myMax, bounds.myMax );
    }

    if ( merged.myMin.x > merged.myMax.x ) {
        merged.myMin = glm::vec3( -8.0f );
        merged.myMax = glm::vec3( 8.0f );
    }
    return merged;
}

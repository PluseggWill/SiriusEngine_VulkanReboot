#include "Gfx_Bounds.h"

#include <algorithm>

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

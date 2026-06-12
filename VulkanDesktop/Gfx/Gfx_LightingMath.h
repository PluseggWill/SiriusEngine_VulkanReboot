#pragma once

#include "Vk_Camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Gfx_LightingMath {

inline std::array< glm::vec3, 8 > FrustumCornersWorld( const glm::mat4& aInvViewProj ) {
    std::array< glm::vec3, 8 > corners{};
    size_t                       index = 0;
    for ( int zSign : { -1, 1 } ) {
        for ( int ySign : { -1, 1 } ) {
            for ( int xSign : { -1, 1 } ) {
                const glm::vec4 clip = glm::vec4( static_cast< float >( xSign ), static_cast< float >( ySign ), static_cast< float >( zSign ), 1.0f );
                const glm::vec4 world = aInvViewProj * clip;
                corners[ index++ ]    = glm::vec3( world ) / world.w;
            }
        }
    }
    return corners;
}

inline glm::mat4 ComputeDirectionalShadowMatrix( const glm::vec3& aSunDirectionTowardLight, const Vk_Camera& aCamera, float aShadowMapSize, float aPadding = 8.0f ) {
    const glm::mat4 invViewProj = glm::inverse( aCamera.myProj * aCamera.myView );
    const auto      corners     = FrustumCornersWorld( invViewProj );

    glm::vec3 center( 0.0f );
    for ( const glm::vec3& corner : corners ) {
        center += corner;
    }
    center /= static_cast< float >( corners.size() );

    const glm::vec3 lightDir = glm::normalize( aSunDirectionTowardLight );
    glm::vec3       up       = glm::vec3( 0.0f, 0.0f, 1.0f );
    if ( std::abs( glm::dot( up, lightDir ) ) > 0.95f ) {
        up = glm::vec3( 0.0f, 1.0f, 0.0f );
    }

    const float     lightDistance = std::max( aPadding * 4.0f, 50.0f );
    const glm::vec3 lightPos      = center - lightDir * lightDistance;
    const glm::mat4 lightView     = glm::lookAt( lightPos, center, up );

    glm::vec3 minBounds( std::numeric_limits< float >::max() );
    glm::vec3 maxBounds( std::numeric_limits< float >::lowest() );
    for ( const glm::vec3& corner : corners ) {
        const glm::vec3 lightSpace = glm::vec3( lightView * glm::vec4( corner, 1.0f ) );
        minBounds                  = glm::min( minBounds, lightSpace );
        maxBounds                  = glm::max( maxBounds, lightSpace );
    }

    minBounds.z -= aPadding;
    maxBounds.z += aPadding;

    const float worldUnitsPerTexel = ( maxBounds.x - minBounds.x ) / aShadowMapSize;
    if ( worldUnitsPerTexel > 0.0f ) {
        minBounds.x = std::floor( minBounds.x / worldUnitsPerTexel ) * worldUnitsPerTexel;
        minBounds.y = std::floor( minBounds.y / worldUnitsPerTexel ) * worldUnitsPerTexel;
        maxBounds.x = std::floor( maxBounds.x / worldUnitsPerTexel ) * worldUnitsPerTexel;
        maxBounds.y = std::floor( maxBounds.y / worldUnitsPerTexel ) * worldUnitsPerTexel;
    }

    const glm::mat4 lightProj = glm::ortho( minBounds.x, maxBounds.x, minBounds.y, maxBounds.y, minBounds.z, maxBounds.z );
    return lightProj * lightView;
}

}  // namespace Gfx_LightingMath

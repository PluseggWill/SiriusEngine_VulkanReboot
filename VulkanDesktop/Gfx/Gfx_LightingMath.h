#pragma once

#include "Gfx_Bounds.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Gfx_LightingMath {

// Khronos Vulkan-Samples OrthographicCamera + shadows/main.frag contract.
// Reversed depth: glm::ortho(l, r, b, t, far_plane, near_plane) — near objects map to ~1.0 in the shadow map.

struct Gfx_KhronosShadowOrtho {
    float myLeft   = -1.0f;
    float myRight  = 1.0f;
    float myBottom = -1.0f;
    float myTop    = 1.0f;
    float myNear   = 0.1f;  // positive distance to near clip plane in light view
    float myFar    = 100.0f;
};

inline glm::mat4 Gfx_VulkanStyleProjection( glm::mat4 aProjection ) {
    aProjection[ 1 ][ 1 ] *= -1.0f;
    return aProjection;
}

inline glm::mat4 Gfx_ComputeDirectionalLightView( const glm::vec3& aSunDirectionTowardLight, const glm::vec3& aFocusCenter ) {
    const glm::vec3 lightDir = glm::normalize( aSunDirectionTowardLight );
    glm::vec3       up       = glm::vec3( 0.0f, 0.0f, 1.0f );
    if ( std::abs( glm::dot( up, lightDir ) ) > 0.95f ) {
        up = glm::vec3( 0.0f, 1.0f, 0.0f );
    }

    const float     sceneReach    = 64.0f;
    const glm::vec3 lightPosition = aFocusCenter - lightDir * sceneReach;
    return glm::lookAt( lightPosition, aFocusCenter, up );
}

inline Gfx_KhronosShadowOrtho Gfx_ComputeShadowOrthoFromSceneBounds( const Gfx_Bounds& aSceneBounds, const glm::mat4& aLightView, float aPadding = 8.0f ) {
    const auto corners = Gfx_BoundsCorners( aSceneBounds );

    glm::vec3 minBounds( std::numeric_limits< float >::max() );
    glm::vec3 maxBounds( std::numeric_limits< float >::lowest() );
    for ( const glm::vec3& corner : corners ) {
        const glm::vec3 lightSpace = glm::vec3( aLightView * glm::vec4( corner, 1.0f ) );
        minBounds                  = glm::min( minBounds, lightSpace );
        maxBounds                  = glm::max( maxBounds, lightSpace );
    }

    minBounds -= glm::vec3( aPadding );
    maxBounds += glm::vec3( aPadding );

    Gfx_KhronosShadowOrtho ortho{};
    ortho.myLeft   = minBounds.x;
    ortho.myRight  = maxBounds.x;
    ortho.myBottom = minBounds.y;
    ortho.myTop    = maxBounds.y;
    ortho.myNear   = -maxBounds.z;
    ortho.myFar    = -minBounds.z;
    return ortho;
}

inline glm::mat4 Gfx_ComputeKhronosShadowMatrix( const glm::mat4& aLightView, const Gfx_KhronosShadowOrtho& aOrtho ) {
    // vkb::sg::OrthographicCamera::get_projection — far/near swapped for reversed depth buffer.
    glm::mat4 lightProjection = glm::ortho( aOrtho.myLeft, aOrtho.myRight, aOrtho.myBottom, aOrtho.myTop, aOrtho.myFar, aOrtho.myNear );
    lightProjection           = Gfx_VulkanStyleProjection( lightProjection );
    return lightProjection * aLightView;
}

inline glm::mat4 Gfx_ComputeKhronosDirectionalShadowMatrixFromScene( const glm::vec3& aSunDirectionTowardLight, const Gfx_Bounds& aSceneBounds ) {
    const glm::vec3              focusCenter = Gfx_BoundsCenter( aSceneBounds );
    const glm::mat4              lightView   = Gfx_ComputeDirectionalLightView( aSunDirectionTowardLight, focusCenter );
    const Gfx_KhronosShadowOrtho ortho       = Gfx_ComputeShadowOrthoFromSceneBounds( aSceneBounds, lightView );
    return Gfx_ComputeKhronosShadowMatrix( lightView, ortho );
}

}  // namespace Gfx_LightingMath

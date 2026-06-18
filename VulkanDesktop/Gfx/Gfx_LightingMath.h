#pragma once

#include "Gfx_Bounds.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

struct Gfx_DirectionalShadowSetup {
    glm::mat4 myLightViewProj{ 1.0f };
    float     myLightSpaceDepthRange = 1.0f;
    float     myWorldTexelSize       = 1.0f;   // max(orthoWidth, orthoHeight) / shadowMapSize
    float     myNormalBias           = 0.0f;   // world-space vertex normal offset (1–2 texels)
    float     myDepthBiasConstant    = 0.0f;   // scaled depthBiasConstantFactor for vkCmdSetDepthBias
    float     myDepthBiasSlope       = 0.0f;   // scaled depthBiasSlopeFactor for vkCmdSetDepthBias
};

struct Gfx_LightSpaceBounds {
    glm::vec3 myMin{ std::numeric_limits< float >::max() };
    glm::vec3 myMax{ std::numeric_limits< float >::lowest() };
};

// Default sun for Z-up outdoor scenes (surface → sun): high noon with slight +X/+Y bias.
inline glm::vec3 Gfx_DefaultSunDirectionTowardLight() {
    return glm::normalize( glm::vec3( 0.1f, 0.2f, 1.0f ) );
}

// Z-up v0 directional shadow map is fit for overhead sun only (Khronos outdoor contract).
inline bool Gfx_IsSunElevationValidForShadows( const glm::vec3& aSunDirectionTowardLight ) {
    constexpr float kMinSunUpDot = 0.08f;  // reject below-horizon / grazing (surface→sun must have +Z)
    return aSunDirectionTowardLight.z > kMinSunUpDot;
}

inline bool Gfx_ShouldCompareDirectionalShadows( bool aShadowsEnabled, const glm::vec3& aSunDirectionTowardLight ) {
    return aShadowsEnabled && Gfx_IsSunElevationValidForShadows( aSunDirectionTowardLight );
}

inline glm::mat4 Gfx_VulkanStyleProjection( glm::mat4 aProjection ) {
    aProjection[ 1 ][ 1 ] *= -1.0f;
    return aProjection;
}

inline float Gfx_ComputeDirectionalLightReach( const Gfx_Bounds& aSceneBounds ) {
    const glm::vec3 extent    = aSceneBounds.myMax - aSceneBounds.myMin;
    const float     maxExtent = std::max( extent.x, std::max( extent.y, extent.z ) );
    return std::max( maxExtent * 2.5f, 16.0f );
}

inline glm::mat4 Gfx_ComputeDirectionalLightView( const glm::vec3& aSunDirectionTowardLight, const glm::vec3& aFocusCenter, float aSceneReach ) {
    // aSunDirectionTowardLight = surface → sun (same as cluster / Pbr_EvalDirect L).
    // Shadow pass must look along incoming light (-sunDir), so place the eye on the sun side of the focus.
    const glm::vec3 lightDir = glm::normalize( aSunDirectionTowardLight );

    glm::vec3 up = glm::vec3( 0.0f, 0.0f, 1.0f );
    // Z-up outdoor sun is often near +Z; avoid lookAt degeneracy when up ≈ view (-sunDir).
    if ( std::abs( glm::dot( up, lightDir ) ) >= 0.85f ) {
        up = glm::vec3( 0.0f, 1.0f, 0.0f );
    }

    const glm::vec3 lightPosition = aFocusCenter + lightDir * aSceneReach;
    return glm::lookAt( lightPosition, aFocusCenter, up );
}

inline float Gfx_ComputeShadowOrthoPadding( const Gfx_Bounds& aSceneBounds ) {
    const glm::vec3 extent    = aSceneBounds.myMax - aSceneBounds.myMin;
    const float     maxExtent = std::max( extent.x, std::max( extent.y, extent.z ) );
    return std::max( 0.005f, maxExtent * 0.15f );  // was 0.08 — too tight, caused border-lit light leaks at frustum edges
}

inline void Gfx_ExpandLightSpaceBounds( Gfx_LightSpaceBounds& aBounds, const glm::mat4& aLightView, const glm::vec3& aWorldPoint ) {
    const glm::vec3 lightSpace = glm::vec3( aLightView * glm::vec4( aWorldPoint, 1.0f ) );
    aBounds.myMin              = glm::min( aBounds.myMin, lightSpace );
    aBounds.myMax              = glm::max( aBounds.myMax, lightSpace );
}

inline glm::mat4 Gfx_ComputeKhronosShadowMatrix( const glm::mat4& aLightView, const Gfx_KhronosShadowOrtho& aOrtho ) {
    glm::mat4 lightProjection = glm::ortho( aOrtho.myLeft, aOrtho.myRight, aOrtho.myBottom, aOrtho.myTop, aOrtho.myFar, aOrtho.myNear );
    lightProjection           = Gfx_VulkanStyleProjection( lightProjection );
    return lightProjection * aLightView;
}

inline float Gfx_ComputeShadowDepthBiasConstant( float /*aLightSpaceDepthRange*/ ) {
    // Khronos shadow subpass uses fixed depth-buffer bias (-1.4, -1.7), not scaled by world ortho extent.
    constexpr float kKhronosDepthBiasConstant = -1.4f;
    return kKhronosDepthBiasConstant;
}

// Map GLM OpenGL clip Z ([-1, 1]) to Vulkan depth-compare reference ([0, 1]) — must match shadow pass viewport transform.
inline float Gfx_MapClipZToShadowCompareDepth( float aClipZ ) {
    return aClipZ * 0.5f + 0.5f;
}

// Snap ortho XY to shadow-map texels so the matrix stays stable when the fly camera moves.
inline void Gfx_SnapKhronosShadowOrthoToTexels( Gfx_KhronosShadowOrtho& aOrtho, const glm::mat4& aLightView, const glm::vec3& aSnapFocusWorld, uint32_t aShadowMapSize ) {
    if ( aShadowMapSize == 0 ) {
        return;
    }

    const float orthoWidth  = aOrtho.myRight - aOrtho.myLeft;
    const float orthoHeight = aOrtho.myTop - aOrtho.myBottom;
    if ( orthoWidth <= 0.0f || orthoHeight <= 0.0f ) {
        return;
    }

    const float texelX = orthoWidth / static_cast< float >( aShadowMapSize );
    const float texelY = orthoHeight / static_cast< float >( aShadowMapSize );

    const glm::vec3 focusLight = glm::vec3( aLightView * glm::vec4( aSnapFocusWorld, 1.0f ) );
    const float     snappedX   = std::floor( focusLight.x / texelX ) * texelX;
    const float     snappedY   = std::floor( focusLight.y / texelY ) * texelY;
    const float     deltaX     = snappedX - focusLight.x;
    const float     deltaY     = snappedY - focusLight.y;

    aOrtho.myLeft += deltaX;
    aOrtho.myRight += deltaX;
    aOrtho.myBottom += deltaY;
    aOrtho.myTop += deltaY;
}

// Scene-only directional shadow fit (sun + opaque AABB). Independent of camera pose.
inline Gfx_DirectionalShadowSetup Gfx_ComputeKhronosDirectionalShadowSetup( const glm::vec3& aSunDirectionTowardLight, const Gfx_Bounds& aSceneBounds,
                                                                            uint32_t aShadowMapSize ) {
    const glm::vec3 focusCenter = Gfx_BoundsCenter( aSceneBounds );
    const float     sceneReach  = Gfx_ComputeDirectionalLightReach( aSceneBounds );
    const glm::mat4 lightView   = Gfx_ComputeDirectionalLightView( aSunDirectionTowardLight, focusCenter, sceneReach );
    const float     padding     = Gfx_ComputeShadowOrthoPadding( aSceneBounds );

    Gfx_LightSpaceBounds sceneLight{};
    for ( const glm::vec3& corner : Gfx_BoundsCorners( aSceneBounds ) ) {
        Gfx_ExpandLightSpaceBounds( sceneLight, lightView, corner );
    }

    Gfx_KhronosShadowOrtho ortho{};
    ortho.myLeft   = sceneLight.myMin.x - padding;
    ortho.myRight  = sceneLight.myMax.x + padding;
    ortho.myBottom = sceneLight.myMin.y - padding;
    ortho.myTop    = sceneLight.myMax.y + padding;
    ortho.myNear   = -sceneLight.myMax.z - padding;
    ortho.myFar    = -sceneLight.myMin.z + padding;

    Gfx_SnapKhronosShadowOrthoToTexels( ortho, lightView, focusCenter, aShadowMapSize );

    Gfx_DirectionalShadowSetup setup{};
    setup.myLightViewProj        = Gfx_ComputeKhronosShadowMatrix( lightView, ortho );
    setup.myLightSpaceDepthRange = std::max( 0.001f, ortho.myFar - ortho.myNear );

    // Scene-scale-aware shadow bias: hardware depth bias factors scaled to ortho depth range.
    // Normal-offset in vertex shader (myNormalBias) is intentionally 0 — it produces texel-grid
    // artifacts on flat surfaces. The orthographic depth bias below provides sufficient guard.
    const float orthoWidth        = ortho.myRight - ortho.myLeft;
    const float orthoHeight       = ortho.myTop - ortho.myBottom;
    const float safeShadowMapSize = static_cast< float >( std::max( 1u, aShadowMapSize ) );
    setup.myWorldTexelSize        = std::max( orthoWidth, orthoHeight ) / safeShadowMapSize;
    setup.myNormalBias            = 0.0f;
    setup.myDepthBiasConstant     = -setup.myLightSpaceDepthRange * 80.0f;  // ~1.5cm world bias (was 10 → 1.8mm, too small)
    setup.myDepthBiasSlope        = -setup.myLightSpaceDepthRange * 40.0f;  // ~2.5mm for 45° surface
    return setup;
}

inline glm::mat4 Gfx_ComputeKhronosDirectionalShadowMatrixFromScene( const glm::vec3& aSunDirectionTowardLight, const Gfx_Bounds& aSceneBounds, uint32_t aShadowMapSize ) {
    return Gfx_ComputeKhronosDirectionalShadowSetup( aSunDirectionTowardLight, aSceneBounds, aShadowMapSize ).myLightViewProj;
}

}  // namespace Gfx_LightingMath

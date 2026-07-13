#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// API-agnostic Halton subpixel jitter for temporal rendering (TAA / history reprojection).
namespace Gfx_TemporalJitter {

constexpr uint32_t kSequenceLength = 16u;

inline float Halton( uint32_t aIndex, uint32_t aBase ) {
    float    result = 0.0f;
    float    factor = 1.0f / static_cast< float >( aBase );
    uint32_t i      = aIndex;
    while ( i > 0 ) {
        result += factor * static_cast< float >( i % aBase );
        i /= aBase;
        factor /= static_cast< float >( aBase );
    }
    return result;
}

// Jitter offset in clip-space units (add to projection [2][0]/[2][1] after Vulkan Y flip).
inline glm::vec2 SampleNdc( uint32_t aHaltonIndex, uint32_t aWidth, uint32_t aHeight ) {
    if ( aWidth == 0 || aHeight == 0 ) {
        return glm::vec2( 0.0f );
    }
    const uint32_t sequenceIndex = ( aHaltonIndex % kSequenceLength ) + 1u;
    const float    jitterX       = Halton( sequenceIndex, 2u ) - 0.5f;
    const float    jitterY       = Halton( sequenceIndex, 3u ) - 0.5f;
    return glm::vec2( ( 2.0f * jitterX ) / static_cast< float >( aWidth ), ( 2.0f * jitterY ) / static_cast< float >( aHeight ) );
}

inline glm::mat4 ApplyToProjection( const glm::mat4& aProjection, const glm::vec2& aJitterNdc ) {
    glm::mat4 jittered = aProjection;
    jittered[ 2 ][ 0 ] += aJitterNdc.x;
    jittered[ 2 ][ 1 ] += aJitterNdc.y;
    return jittered;
}

}  // namespace Gfx_TemporalJitter

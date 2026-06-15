#pragma once

#include "Gfx_LightingMath.h"

#include <glm/glm.hpp>

#include <cstdint>

// std140 UBO — matches LightingGlobals in lit/deferred shaders (Set 0 binding eVk_LightingGlobalsBinding).
struct GpuLightingGlobals {
    alignas( 16 ) glm::mat4 myLightViewProj{};
    alignas( 16 ) glm::vec4 myShadowParams{ 0.0f, 0.0f, 1.0f, 0.0f };  // z = compare active (0/1), w = 1/mapSize
    alignas( 16 ) glm::vec4 myIblParams{ 1.0f, 1.0f, 0.0f, 0.15f };     // x = intensity, y = enabled, z = prefilter max mip, w = specular shadow min
};

static_assert( sizeof( GpuLightingGlobals ) == 96, "GpuLightingGlobals must be std140-compatible (96 bytes)" );

struct Gfx_LightingSettings {
    bool  myShadowsEnabled         = true;
    bool  myIblEnabled             = true;
    float myIblIntensity           = 1.35f;
    float myIblSpecularShadowMin   = 0.15f;  // specular IBL floor in full sun shadow (mix to 1.0 in lit)
};

inline GpuLightingGlobals Gfx_BuildLightingGlobals( const Gfx_LightingSettings& aSettings, const glm::mat4& aLightViewProj, float aPrefilterMaxMipLevel,
                                                    const glm::vec3& aSunDirectionTowardLight, uint32_t aShadowMapSize ) {
    GpuLightingGlobals globals{};
    globals.myLightViewProj  = aLightViewProj;
    globals.myShadowParams.z = Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aSettings.myShadowsEnabled, aSunDirectionTowardLight ) ? 1.0f : 0.0f;
    globals.myShadowParams.w = aShadowMapSize > 0u ? 1.0f / static_cast< float >( aShadowMapSize ) : 0.0f;
    globals.myIblParams.x    = aSettings.myIblIntensity;
    globals.myIblParams.y    = aSettings.myIblEnabled ? 1.0f : 0.0f;
    globals.myIblParams.z    = aPrefilterMaxMipLevel;
    globals.myIblParams.w    = aSettings.myIblSpecularShadowMin;
    return globals;
}

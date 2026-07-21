#pragma once

#include "Gfx_LightingMath.h"

#include "../RenderContract/Gpu_LightingGlobals.h"

#include <glm/glm.hpp>

using Gfx_LightingSettings = Gpu_LightingSettings;

// Gfx layer owns directional-shadow compare policy (sun direction + runtime toggles); RenderContract remains dependency-free.
inline Gpu_LightingGlobals Gfx_BuildLightingGlobals( const Gfx_LightingSettings& aSettings, const glm::mat4& aLightViewProj, float aPrefilterMaxMipLevel,
                                                     const glm::vec3& aSunDirectionTowardLight, uint32_t aShadowMapSize ) {
    const bool enableShadowCompare = Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aSettings.myShadowsEnabled, aSunDirectionTowardLight );
    return Gpu_BuildLightingGlobals( aSettings, aLightViewProj, aPrefilterMaxMipLevel, enableShadowCompare, aShadowMapSize );
}

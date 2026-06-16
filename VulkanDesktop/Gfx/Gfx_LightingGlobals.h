#pragma once

#include "../RenderContract/GpuLightingGlobals.h"

#include <glm/glm.hpp>

using Gfx_LightingSettings = GpuLightingSettings;

inline GpuLightingGlobals Gfx_BuildLightingGlobals( const Gfx_LightingSettings& aSettings, const glm::mat4& aLightViewProj, float aPrefilterMaxMipLevel,
                                                    const glm::vec3& aSunDirectionTowardLight, uint32_t aShadowMapSize ) {
    return Gpu_BuildLightingGlobals( aSettings, aLightViewProj, aPrefilterMaxMipLevel, aSunDirectionTowardLight, aShadowMapSize );
}

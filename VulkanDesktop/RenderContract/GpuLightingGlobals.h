#pragma once

#include <glm/glm.hpp>

#include <cstdint>

// std140 UBO — Set 0 binding eVk_LightingGlobalsBinding.
// myShadowParams: x = SSR enabled (0/1), y = specular occlusion enabled (0/1), z = shadow compare, w = 1/shadowMapSize.
struct GpuLightingGlobals {
    alignas( 16 ) glm::mat4 myLightViewProj{};
    alignas( 16 ) glm::vec4 myShadowParams{ 0.0f, 0.0f, 1.0f, 0.0f };
    alignas( 16 ) glm::vec4 myIblParams{ 1.0f, 1.0f, 0.0f, 0.0f };
};

static_assert( sizeof( GpuLightingGlobals ) == 96, "GpuLightingGlobals must be std140-compatible (96 bytes)" );

struct GpuLightingSettings {
    bool      myShadowsEnabled              = true;
    bool      myIblEnabled                  = true;
    float     myIblIntensity                = 1.35f;
    bool      mySpecularOcclusionEnabled    = true;
    bool      mySsrEnabled                  = false;
    float     mySsrMaxRoughness             = 0.35f;
    float     mySsrMaxDistance              = 40.0f;
    float     mySsrThickness                = 0.05f;
    uint32_t  mySsrMaxSteps                 = 32u;
    float     mySsrHistoryDepthReject       = 0.15f;  // temporal SSR: world-space reject sigma (see Ssr_SampleLitHdrHistory)
    bool      mySpecularOcclusionUseCones   = false;  // Phase C: bent-normal cones (requires GTAO AO method)
    bool      myLocalReflectionProbeEnabled = false;  // Phase C: parallax box probe (mutually exclusive with DDGI volume fields)
    float     myLocalProbeIntensity         = 1.0f;
    glm::vec3 myLocalProbeCenter{ 0.0f, 2.5f, 0.0f };
    float     myLocalProbePad0 = 0.0f;
    glm::vec3 myLocalProbeExtents{ 12.0f, 6.0f, 12.0f };
    float     myLocalProbePad1      = 0.0f;
    bool      myDdgiEnabled         = false;
    bool      myDdgiStaggeredUpdate = true;
    float     myDdgiIntensity       = 1.0f;
    float     myDdgiDebugOverlay    = 0.0f;
    float     myDdgiHistoryBlend    = 0.9f;
    uint32_t  myDdgiProbeCountX     = 12u;
    uint32_t  myDdgiProbeCountY     = 8u;
    uint32_t  myDdgiProbeCountZ     = 12u;
    uint32_t  myDdgiUpdateBudget    = 64u;
    glm::vec3 myDdgiVolumeCenter{ 0.0f, 0.0f, 1.0f };
    float     myDdgiVolumePad0 = 0.0f;
    glm::vec3 myDdgiVolumeExtents{ 20.0f, 20.0f, 8.0f };
    float     myDdgiVolumePad1 = 0.0f;
};

// Contract helper: caller provides shadow-compare decision to keep RenderContract independent from higher-level lighting math.
inline GpuLightingGlobals Gpu_BuildLightingGlobals( const GpuLightingSettings& aSettings, const glm::mat4& aLightViewProj, float aPrefilterMaxMipLevel,
                                                     bool aEnableShadowCompare, uint32_t aShadowMapSize ) {
    GpuLightingGlobals globals{};
    globals.myLightViewProj  = aLightViewProj;
    globals.myShadowParams.x = aSettings.mySsrEnabled ? 1.0f : 0.0f;
    globals.myShadowParams.y = aSettings.mySpecularOcclusionEnabled ? 1.0f : 0.0f;
    globals.myShadowParams.z = aEnableShadowCompare ? 1.0f : 0.0f;
    globals.myShadowParams.w = aShadowMapSize > 0u ? 1.0f / static_cast< float >( aShadowMapSize ) : 0.0f;
    globals.myIblParams.x    = aSettings.myIblIntensity;
    globals.myIblParams.y    = aSettings.myIblEnabled ? 1.0f : 0.0f;
    globals.myIblParams.z    = aPrefilterMaxMipLevel;
    return globals;
}

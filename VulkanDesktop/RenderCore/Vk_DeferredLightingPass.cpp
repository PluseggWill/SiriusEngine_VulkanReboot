// Module: Vk_DeferredLightingPass — FG v0 clustered deferred resolve to swapchain.
// Set 0: G-buffer samplers + ClusterBuild SSBOs (lights, per-frame cluster lists). Layouts/pool/sets/sampler
// and the DDGI atlas now live in Gfx_DeferredLightingPass / Gfx_DdgiPass; this TU keeps SPIR-V load, Vk
// mirrors, cross-pass TextureAdopt/BufferAdopt, and Record orchestration (push constants, DDGI CPU state).
#include "Vk_DeferredLightingPass.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Gfx/Gfx_DdgiPass.h"
#include "../Gfx/Gfx_DeferredLightingPass.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_AoPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_SsrPass.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_DeferredLightingPass::kMaxFramesInFlight ),
               "Gfx_DeferredLightingPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );
static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_DdgiPass::kMaxFramesInFlight ), "Gfx_DdgiPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace {

constexpr char kDeferredVertSpv[]    = "VulkanDesktop/Shader_Generated/DeferredLightingVert.spv";
constexpr char kDeferredFragSpv[]    = "VulkanDesktop/Shader_Generated/DeferredLightingFrag.spv";
constexpr char kDdgiProbeUpdateSpv[] = "VulkanDesktop/Shader_Generated/DdgiProbeUpdate.spv";

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

void ClearDdgiCpu( Vk_DeferredLightingState& aState ) {
    aState.myDdgiAtlasReadable = false;
}

bool CreateGfxPipelines( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice || !aCore.myPostProcessState.myRhiHybridRenderPass ) {
        return false;
    }
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;

    const std::string         vertPath     = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredVertSpv );
    const std::string         fragPath     = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredFragSpv );
    const std::vector< char > deferredVert = LoadSpirvBytes( vertPath );
    const std::vector< char > deferredFrag = LoadSpirvBytes( fragPath );

    Gfx_DeferredLightingPass::DestroyPipeline( rhi, state.myDeferredGfx );
    Gfx_DeferredLightingPass::PipelineInitDesc deferredDesc{};
    deferredDesc.myVertSpirv      = deferredVert.data();
    deferredDesc.myVertSpirvBytes = deferredVert.size();
    deferredDesc.myFragSpirv      = deferredFrag.data();
    deferredDesc.myFragSpirvBytes = deferredFrag.size();
    deferredDesc.myRenderPass     = aCore.myPostProcessState.myRhiHybridRenderPass;
    deferredDesc.mySampleCount    = 1;  // hybrid resolve color attachment is single-sample
    deferredDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_DeferredLightingPass::CreatePipeline( rhi, deferredDesc, state.myDeferredGfx ) ) {
        return false;
    }

    const std::string         ddgiPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDdgiProbeUpdateSpv );
    const std::vector< char > ddgiSpv  = LoadSpirvBytes( ddgiPath );

    Gfx_DdgiPass::DestroyPipeline( rhi, state.myDdgiGfx );
    Gfx_DdgiPass::PipelineInitDesc ddgiDesc{};
    ddgiDesc.mySpirvCode      = ddgiSpv.data();
    ddgiDesc.mySpirvBytes     = ddgiSpv.size();
    ddgiDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_DdgiPass::CreatePipeline( rhi, ddgiDesc, state.myDdgiGfx ) ) {
        Gfx_DeferredLightingPass::DestroyPipeline( rhi, state.myDeferredGfx );
        return false;
    }

    UtilLogger::Info( "PIPELINE", "DeferredLighting graphics + DDGI compute pipelines created (Gfx)." );
    return true;
}

// Atlas layout: width = probeCountX * probeCountY, height = probeCountZ.
bool CreateOrRefreshDdgiImages( Vk_Renderer& aCore ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;

    // Always rebuild: probe counts may have changed and the CPU-side history must reset with the atlas.
    Gfx_DdgiPass::DestroyImages( rhi, state.myDdgiGfx );

    state.myDdgiProbeCountX     = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountX );
    state.myDdgiProbeCountY     = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountY );
    state.myDdgiProbeCountZ     = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountZ );
    state.myDdgiTotalProbeCount = state.myDdgiProbeCountX * state.myDdgiProbeCountY * state.myDdgiProbeCountZ;

    Gfx_DdgiPass::ImageInitDesc imageDesc{};
    imageDesc.myProbeCountX = state.myDdgiProbeCountX;
    imageDesc.myProbeCountY = state.myDdgiProbeCountY;
    imageDesc.myProbeCountZ = state.myDdgiProbeCountZ;
    if ( !Gfx_DdgiPass::CreateOrRecreateImages( rhi, imageDesc, state.myDdgiGfx ) ) {
        return false;
    }

    state.myDdgiAtlasReadable     = false;
    state.myDdgiHistoryForceReset = true;
    state.myDdgiPrevVolumeCenter  = aCore.myLightingSettings.myDdgiVolumeCenter;
    state.myDdgiPrevVolumeExtents = aCore.myLightingSettings.myDdgiVolumeExtents;
    state.myDdgiPrevCameraEye     = aCore.myPrimaryCamera.myEye;
    return true;
}

// Deferred binding 13 (aoMap) selection: contact-soft ping > raw AO > 1x1 fallback contact.
// All three sources are already Gfx-owned Rhi_Texture — no Vk adopt required.
Rhi_Texture SelectDeferredContactTexture( const Vk_Renderer& aCore ) {
    const Gfx_AoSettings&       aoSettings = aCore.myAoSettings;
    const Vk_ShadowAoSoftState& softState  = aCore.myShadowAoSoftState;

    if ( aoSettings.myContactSoftEnabled && softState.myInitialized ) {
        return softState.myGfx.mySoftPing;
    }
    if ( aoSettings.myEnabled && aCore.myAoState.myInitialized ) {
        return aCore.myAoState.myGfx.myAoRaw;
    }
    if ( softState.myInitialized ) {
        return softState.myGfx.myFallbackContact;
    }
    return {};
}

// Per in-flight frame: cluster-list SSBO differs; G-buffer/IBL/shadow views refresh on resize.
void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !state.myDeferredGfx.mySetsAllocated ) {
        return;
    }

    // Temporary adopts for cross-pass Vk_TextureResource-backed resources (G-buffer, shadow map, IBL).
    Rhi_Texture albedoTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myAlbedo.Image(), aCore.myGBufferState.myAlbedo.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );
    Rhi_Texture normalTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture worldPosTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), aCore.FindDepthFormat(), 1 );
    Rhi_Texture motionVectorTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myMotionVector.Image(), aCore.myGBufferState.myMotionVector.ImageView(), VK_FORMAT_R16G16_SFLOAT, 1 );

    Rhi_Texture irradianceTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myIblResourcesState.myIrradiance.Image(), aCore.myIblResourcesState.myIrradiance.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );
    Rhi_Texture prefilterTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myIblResourcesState.myPrefilter.Image(), aCore.myIblResourcesState.myPrefilter.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );
    Rhi_Texture skyTex = RhiVulkan::TextureAdopt( rhi, aCore.myIblResourcesState.mySky.Image(), aCore.myIblResourcesState.mySky.ImageView(), VK_FORMAT_R8G8B8A8_SRGB, 1 );
    Rhi_Texture brdfLutTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myIblResourcesState.myBrdfLut.Image(), aCore.myIblResourcesState.myBrdfLut.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );

    const Rhi_Sampler iblCubemapSampler = RhiVulkan::SamplerAdopt( aCore.myIblResourcesState.myCubemapSampler );
    const Rhi_Sampler brdfLutSampler    = RhiVulkan::SamplerAdopt( aCore.myIblResourcesState.myBrdfLutSampler );

    const Rhi_Buffer lightingGlobalsBuf = RhiVulkan::BufferAdopt( aCore.myLightingGlobalsBuffer.myBuffer );

    Gfx_DeferredLightingPass::DescriptorUpdateDesc desc{};
    desc.myFrameIndex                 = aFrameIndex;
    desc.myAlbedo                     = albedoTex;
    desc.myNormalRoughness            = normalTex;
    desc.myWorldPos                   = worldPosTex;
    desc.myDepth                      = depthTex;
    desc.myLightsBuffer               = aCore.myClusterBuildState.myGfx.myLightsBuffer;
    desc.myLightsRangeBytes           = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;
    desc.myClusterListBuffer          = aCore.myClusterBuildState.myGfx.myClusterListBuffers[ aFrameIndex ];
    desc.myLightingGlobals            = lightingGlobalsBuf;
    desc.myLightingGlobalsOffsetBytes = aCore.PadUniformBufferSize( sizeof( Gpu_LightingGlobals ) ) * aFrameIndex;
    desc.myLightingGlobalsRangeBytes  = sizeof( Gpu_LightingGlobals );
    desc.myShadowDepth                = aCore.myShadowMapState.myGfx.myDepth;
    desc.myShadowCompareSampler       = aCore.myShadowMapState.myGfx.myCompareSampler;
    desc.myShadowDepthRead            = aCore.myShadowMapState.myGfx.myDepth;
    desc.myShadowDepthReadSampler     = aCore.myShadowMapState.myGfx.myDepthReadSampler;
    desc.myIrradianceIbl              = irradianceTex;
    desc.myPrefilterIbl               = prefilterTex;
    desc.myBrdfLut                    = brdfLutTex;
    desc.myIblCubemapSampler          = iblCubemapSampler;
    desc.myBrdfLutSampler             = brdfLutSampler;
    desc.mySky                        = skyTex;
    desc.myAo                         = SelectDeferredContactTexture( aCore );
    desc.myHiZ                        = aCore.myDepthPyramidState.myGfx.myPyramid;
    desc.myHiZSampler                 = aCore.myDepthPyramidState.myGfx.myPyramidSampler;
    desc.myDdgiIrradiance             = state.myDdgiGfx.myIrradianceAtlas;
    desc.myDdgiVisibility             = state.myDdgiGfx.myVisibilityAtlas;
    desc.mySsr                        = aCore.mySsrState.myGfx.mySsrOutput;
    desc.myBentNormal                 = aCore.myAoState.myGfx.myBentNormalHalf;
    desc.myMotionVector               = motionVectorTex;
    desc.myFallback                   = albedoTex;
    Gfx_DeferredLightingPass::UpdateDescriptors( rhi, desc, state.myDeferredGfx );

    Rhi::DeviceDestroyTexture( rhi, albedoTex );
    Rhi::DeviceDestroyTexture( rhi, normalTex );
    Rhi::DeviceDestroyTexture( rhi, worldPosTex );
    Rhi::DeviceDestroyTexture( rhi, depthTex );
    Rhi::DeviceDestroyTexture( rhi, motionVectorTex );
    Rhi::DeviceDestroyTexture( rhi, irradianceTex );
    Rhi::DeviceDestroyTexture( rhi, prefilterTex );
    Rhi::DeviceDestroyTexture( rhi, skyTex );
    Rhi::DeviceDestroyTexture( rhi, brdfLutTex );
}

void UpdateDdgiDescriptors( Vk_Renderer& aCore ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myDdgiGfx.mySetsAllocated ) {
        return;
    }

    Rhi_Texture worldPosTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture normalTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Gfx_DdgiPass::DescriptorUpdateDesc desc{};
    desc.myFramesInFlight  = MAX_FRAMES_IN_FLIGHT;
    desc.myWorldPos        = worldPosTex;
    desc.myNormalRoughness = normalTex;
    Gfx_DdgiPass::UpdateDescriptors( rhi, desc, state.myDdgiGfx );

    Rhi::DeviceDestroyTexture( rhi, worldPosTex );
    Rhi::DeviceDestroyTexture( rhi, normalTex );
}

void CreatePipelineResources( Vk_Renderer& aCore ) {
    if ( !Vk_PostProcessPass::HasHybridResolve( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: PostProcess hybrid resolve render pass is required" );
    }

    if ( !CreateGfxPipelines( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx pipeline create failed" );
    }
    if ( !CreateOrRefreshDdgiImages( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx DDGI CreateOrRecreateImages failed" );
    }

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateDescriptorSet( aCore, i );
    }
    UpdateDdgiDescriptors( aCore );
}

Gpu_DeferredLightingPushConstants BuildPushConstants( const Vk_Renderer& aCore ) {
    Gpu_DeferredLightingPushConstants push{};
    const uint32_t                    width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t                    height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    push.tilesX                              = Gfx_ClusterLighting::TilesForExtent( width );
    push.tilesY                              = Gfx_ClusterLighting::TilesForExtent( height );
    push.tileSize                            = Gfx_ClusterLighting::kTileSize;
    std::memcpy( push.ambientColor, glm::value_ptr( aCore.myEnvironmentData.myAmbientColor ), sizeof( float ) * 4 );
    std::memcpy( push.viewWorldPos, glm::value_ptr( aCore.myEnvironmentData.myViewWorldPos ), sizeof( float ) * 4 );
    push.debugView              = aCore.myEnvironmentData.myFogDistance.w;
    push.legacySpecularStrength = aCore.myAoSettings.myEnabled ? 1.0f : 0.0f;
    push.legacyShininess        = aCore.myAoSettings.myIntensity;
    push.legacyPad              = aCore.myAoSettings.myPower;
    push.contactSoftEnabled     = ( aCore.myAoSettings.myContactSoftEnabled && aCore.myShadowAoSoftState.myInitialized ) ? 1.0f : 0.0f;
    push.ddgiEnabled            = aCore.myLightingSettings.myDdgiEnabled ? 1.0f : 0.0f;
    push.ddgiIntensity          = aCore.myLightingSettings.myDdgiIntensity;
    push.ddgiDebugOverlay       = aCore.myLightingSettings.myDdgiDebugOverlay;
    push.ddgiProbeCountX        = aCore.myDeferredLightingState.myDdgiProbeCountX;
    push.ddgiProbeCountY        = aCore.myDeferredLightingState.myDdgiProbeCountY;
    push.ddgiProbeCountZ        = aCore.myDeferredLightingState.myDdgiProbeCountZ;

    // ddgiProbeCounts.w feature flags; ddgiVolume* / contactSoftParams.z repurposed when local probe active (DDGI off).
    uint32_t featureFlags = 0u;
    if ( aCore.myLightingSettings.mySpecularOcclusionUseCones && aCore.myAoSettings.myMethod == Gfx_AoMethod::Gtao ) {
        featureFlags |= 1u;
    }
    const bool localProbeActive = aCore.myLightingSettings.myLocalReflectionProbeEnabled && !aCore.myLightingSettings.myDdgiEnabled;
    if ( localProbeActive ) {
        featureFlags |= 2u;
    }
    push.ddgiProbeCountPad = featureFlags;

    glm::vec3 volumeMin{};
    glm::vec3 volumeMax{};
    if ( localProbeActive ) {
        volumeMin          = aCore.myLightingSettings.myLocalProbeCenter - aCore.myLightingSettings.myLocalProbeExtents;
        volumeMax          = aCore.myLightingSettings.myLocalProbeCenter + aCore.myLightingSettings.myLocalProbeExtents;
        push.ddgiIntensity = aCore.myLightingSettings.myLocalProbeIntensity;
    }
    else if ( aCore.myLightingSettings.myDdgiEnabled ) {
        volumeMin = aCore.myLightingSettings.myDdgiVolumeCenter - aCore.myLightingSettings.myDdgiVolumeExtents;
        volumeMax = aCore.myLightingSettings.myDdgiVolumeCenter + aCore.myLightingSettings.myDdgiVolumeExtents;
    }
    push.ddgiVolumeMin[ 0 ] = volumeMin.x;
    push.ddgiVolumeMin[ 1 ] = volumeMin.y;
    push.ddgiVolumeMin[ 2 ] = volumeMin.z;
    push.ddgiVolumeMin[ 3 ] = 0.0f;
    push.ddgiVolumeMax[ 0 ] = volumeMax.x;
    push.ddgiVolumeMax[ 1 ] = volumeMax.y;
    push.ddgiVolumeMax[ 2 ] = volumeMax.z;
    push.ddgiVolumeMax[ 3 ] = 0.0f;
    push.depthSlice =
        std::min( aCore.myAoSettings.myHiZDebugMip, Vk_DepthPyramidPass::GetMipLevelCount( aCore ) > 0 ? Vk_DepthPyramidPass::GetMipLevelCount( aCore ) - 1 : 0u );

    const glm::mat4 invViewProj = glm::inverse( aCore.myPrimaryCamera.myProj * aCore.myPrimaryCamera.myView );
    std::memcpy( push.invViewProj, glm::value_ptr( invViewProj ), sizeof( push.invViewProj ) );
    return push;
}

void UpdateAoDescriptorBinding( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !state.myDeferredGfx.mySetsAllocated ) {
        return;
    }

    Rhi_Texture albedoTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myAlbedo.Image(), aCore.myGBufferState.myAlbedo.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );

    Gfx_DeferredLightingPass::AoBindingUpdateDesc desc{};
    desc.myFrameIndex = aFrameIndex;
    desc.myAo         = SelectDeferredContactTexture( aCore );
    desc.myFallback   = albedoTex;
    Gfx_DeferredLightingPass::UpdateAoBinding( rhi, desc, state.myDeferredGfx );

    Rhi::DeviceDestroyTexture( rhi, albedoTex );
}

}  // namespace

namespace Vk_DeferredLightingPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_DeferredLightingPass::Destroy( aCore.myGfxRhiDevice, aCore.myDeferredLightingState.myDeferredGfx );
        Gfx_DdgiPass::Destroy( aCore.myGfxRhiDevice, aCore.myDeferredLightingState.myDdgiGfx );
    }
    ClearDdgiCpu( aCore.myDeferredLightingState );
    aCore.myDeferredLightingState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }

    if ( !CreateGfxPipelines( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx pipeline recreate failed" );
    }

    aCore.myDeferredLightingState.myDdgiUpdateCursor = 0u;
    if ( !CreateOrRefreshDdgiImages( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx DDGI CreateOrRecreateImages failed" );
    }

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateDescriptorSet( aCore, i );
    }
    UpdateDdgiDescriptors( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myDeferredLightingState.myInitialized ) {
        for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
            UpdateDescriptorSet( aCore, i );
        }
        return;
    }
    if ( !aCore.myGBufferState.myInitialized || !aCore.myClusterBuildState.myInitialized || !aCore.myAoState.myInitialized || !aCore.myDepthPyramidState.myInitialized
         || !aCore.mySsrState.myInitialized || !aCore.myPostProcessState.myInitialized ) {
        throw std::runtime_error( "Vk_DeferredLightingPass::Init requires GBuffer, ClusterBuild, DepthPyramid, SSR, SSAO, and PostProcess" );
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_DeferredLightingPass::Init requires myGfxRhiDevice" );
    }
    UtilLogger::Info( "FG", "Vk_DeferredLightingPass::Init (Gfx create)." );
    CreatePipelineResources( aCore );
    aCore.myDeferredLightingState.myInitialized = true;
}

void RecordDdgiProbeUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( !state.myInitialized || !aCore.myLightingSettings.myDdgiEnabled || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !state.myDdgiGfx.myPipeline
         || !state.myDdgiGfx.mySets[ aFrameIndex ] || !aCore.myGfxRhiDevice ) {
        return;
    }
    if ( state.myDdgiTotalProbeCount == 0u ) {
        return;
    }

    Gfx_DdgiPass::ProbePush push{};
    push.probeGrid           = glm::uvec4( state.myDdgiProbeCountX, state.myDdgiProbeCountY, state.myDdgiProbeCountZ, state.myDdgiTotalProbeCount );
    const uint32_t budget    = std::max( 1u, std::min( aCore.myLightingSettings.myDdgiUpdateBudget, state.myDdgiTotalProbeCount ) );
    push.updateInfo          = glm::uvec4( aCore.myFrameCtx.myFrameNumber, budget, state.myDdgiUpdateCursor, aCore.myLightingSettings.myDdgiStaggeredUpdate ? 1u : 0u );
    push.ambient             = glm::vec4( glm::vec3( aCore.myEnvironmentData.myAmbientColor ), aCore.myLightingSettings.myDdgiIntensity );
    const bool volumeChanged = glm::length( aCore.myLightingSettings.myDdgiVolumeCenter - state.myDdgiPrevVolumeCenter ) > 0.01f
                               || glm::length( aCore.myLightingSettings.myDdgiVolumeExtents - state.myDdgiPrevVolumeExtents ) > 0.01f;
    const bool cameraJumped = glm::length( aCore.myPrimaryCamera.myEye - state.myDdgiPrevCameraEye ) > 2.5f;
    if ( volumeChanged || cameraJumped ) {
        state.myDdgiHistoryForceReset = true;
    }
    const float historyValid  = ( state.myDdgiAtlasReadable && !state.myDdgiHistoryForceReset ) ? 1.0f : 0.0f;
    push.blend                = glm::vec4( std::clamp( aCore.myLightingSettings.myDdgiHistoryBlend, 0.0f, 0.99f ), historyValid, 0.0f, 0.0f );
    const glm::vec3 volumeMin = aCore.myLightingSettings.myDdgiVolumeCenter - aCore.myLightingSettings.myDdgiVolumeExtents;
    const glm::vec3 volumeMax = aCore.myLightingSettings.myDdgiVolumeCenter + aCore.myLightingSettings.myDdgiVolumeExtents;
    push.volumeMin            = glm::vec4( volumeMin, 0.0f );
    push.volumeMax            = glm::vec4( volumeMax, 0.0f );
    push.integrateParams      = glm::vec4( 16.0f, 25.0f, 3.0f, 0.12f );  // samples, maxDistance, distanceFalloff, minVisibility

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_DdgiPass::GpuResources gpu{};
    gpu.myPipeline        = state.myDdgiGfx.myPipeline;
    gpu.myLayout          = state.myDdgiGfx.myLayout;
    gpu.mySet             = state.myDdgiGfx.mySets[ aFrameIndex ];
    gpu.myIrradianceAtlas = state.myDdgiGfx.myIrradianceAtlas;
    gpu.myVisibilityAtlas = state.myDdgiGfx.myVisibilityAtlas;

    Gfx_DdgiPass::RecordInput input{};
    input.myPush        = push;
    input.myAtlasWidth  = state.myDdgiProbeCountX * state.myDdgiProbeCountY;
    input.myAtlasHeight = state.myDdgiProbeCountZ;
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();

    Gfx_DdgiPass::RecordProbeUpdate( frameCmd.Get(), gpu, input );

    state.myDdgiAtlasReadable     = true;
    state.myDdgiHistoryForceReset = false;
    state.myDdgiPrevVolumeCenter  = aCore.myLightingSettings.myDdgiVolumeCenter;
    state.myDdgiPrevVolumeExtents = aCore.myLightingSettings.myDdgiVolumeExtents;
    state.myDdgiPrevCameraEye     = aCore.myPrimaryCamera.myEye;

    if ( aCore.myLightingSettings.myDdgiStaggeredUpdate ) {
        state.myDdgiUpdateCursor = ( state.myDdgiUpdateCursor + budget ) % state.myDdgiTotalProbeCount;
    }
}

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    static bool sDrawLoggedOnce = false;

    UpdateAoDescriptorBinding( aCore, aFrameIndex );

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_DeferredLightingPass::GpuResources gpu{};
    gpu.myPipeline = state.myDeferredGfx.myPipeline;
    gpu.myLayout   = state.myDeferredGfx.myLayout;
    gpu.mySet      = state.myDeferredGfx.mySets[ aFrameIndex ];

    Gfx_DeferredLightingPass::RecordInput input{};
    input.myPush        = BuildPushConstants( aCore );
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();

    Gfx_DeferredLightingPass::RecordDraw( frameCmd.Get(), gpu, input );

    if ( !sDrawLoggedOnce ) {
        UtilLogger::Info( "FG",
                          "DeferredLighting resolve: tiles=" + std::to_string( input.myPush.tilesX ) + "x" + std::to_string( input.myPush.tilesY ) + " (depth slice 0 stub)" );
        sDrawLoggedOnce = true;
    }
}

}  // namespace Vk_DeferredLightingPass

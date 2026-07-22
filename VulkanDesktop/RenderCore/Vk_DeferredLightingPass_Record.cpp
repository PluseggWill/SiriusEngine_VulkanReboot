// Module: Vk_DeferredLightingPass record path (DDGI probe update + fullscreen resolve).
#include "Vk_DeferredLightingPass.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Gfx/Gfx_DdgiPass.h"
#include "../Gfx/Gfx_DeferredLightingPass.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Util/Util_Logger.h"
#include "Vk_AoPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowAoSoftPass.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace {

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

}  // namespace

namespace Vk_DeferredLightingPass {

static void UpdateAoDescriptorBinding( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( aFrameIndex >= MAX_FRAMES_IN_FLIGHT || state.myDescriptorSets[ aFrameIndex ] == VK_NULL_HANDLE ) {
        return;
    }

    VkImageView contactView = Vk_ShadowAoSoftPass::GetDeferredContactMapView( aCore );
    if ( contactView == VK_NULL_HANDLE ) {
        contactView = aCore.myGBufferState.myAlbedo.ImageView();
    }

    VkDescriptorImageInfo aoInfo{};
    aoInfo.sampler     = state.myGBufferSampler;
    aoInfo.imageView   = contactView;
    aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkWriteDescriptorSet write =
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &aoInfo, 13, 1 );
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, 1, &write, 0, nullptr );
}

void RecordDdgiProbeUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( !state.myInitialized || !aCore.myLightingSettings.myDdgiEnabled || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || state.myDdgiProbeUpdatePipeline == VK_NULL_HANDLE
         || state.myDdgiProbeDescriptorSets[ aFrameIndex ] == VK_NULL_HANDLE || !aCore.myGfxRhiDevice ) {
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

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_DdgiPass::GpuResources gpu{};
    gpu.myPipeline        = RhiVulkan::PipelineAdopt( state.myDdgiProbeUpdatePipeline );
    gpu.myLayout          = RhiVulkan::PipelineLayoutAdopt( state.myDdgiProbeUpdatePipelineLayout );
    gpu.mySet             = RhiVulkan::DescriptorSetAdopt( state.myDdgiProbeDescriptorSets[ aFrameIndex ] );
    gpu.myIrradianceAtlas = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myDdgiProbeIrradianceAtlas.Image(), state.myDdgiProbeIrradianceAtlas.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    gpu.myVisibilityAtlas =
        RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myDdgiProbeVisibilityAtlas.Image(), state.myDdgiProbeVisibilityAtlas.ImageView(), VK_FORMAT_R16_SFLOAT, 1 );

    Gfx_DdgiPass::RecordInput input{};
    input.myPush        = push;
    input.myAtlasWidth  = state.myDdgiProbeCountX * state.myDdgiProbeCountY;
    input.myAtlasHeight = state.myDdgiProbeCountZ;
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();

    Gfx_DdgiPass::RecordProbeUpdate( cmd, gpu, input );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myIrradianceAtlas );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myVisibilityAtlas );
    Rhi::CommandListDestroy( cmd );

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

    // Descriptor write stays in RenderCore (Vulkan update API).
    UpdateAoDescriptorBinding( aCore, aFrameIndex );

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_DeferredLightingPass::GpuResources gpu{};
    gpu.myPipeline = RhiVulkan::PipelineAdopt( state.myPipeline );
    gpu.myLayout   = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.mySet      = RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ] );

    Gfx_DeferredLightingPass::RecordInput input{};
    input.myPush        = BuildPushConstants( aCore );
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();

    Gfx_DeferredLightingPass::RecordDraw( cmd, gpu, input );

    Rhi::CommandListDestroy( cmd );

    if ( !sDrawLoggedOnce ) {
        UtilLogger::Info( "FG",
                          "DeferredLighting resolve: tiles=" + std::to_string( input.myPush.tilesX ) + "x" + std::to_string( input.myPush.tilesY ) + " (depth slice 0 stub)" );
        sDrawLoggedOnce = true;
    }
}

}  // namespace Vk_DeferredLightingPass

// Module: Vk_DeferredLightingPass — FG v0 clustered deferred resolve to swapchain.
// Set 0: G-buffer samplers + ClusterBuild SSBOs (lights, per-frame cluster lists).
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
#include "Vk_Initializer.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_SsrPass.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

void SyncPipelineMirrors( Vk_Renderer& aCore ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    Rhi_Device&               rhi   = aCore.myGfxRhiDevice;
    state.myPipeline                = RhiVulkan::PipelineGetVk( rhi, state.myDeferredGfx.myPipeline );
    state.myDdgiProbeUpdatePipeline = RhiVulkan::PipelineGetVk( rhi, state.myDdgiGfx.myPipeline );
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
    deferredDesc.myLayout         = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    deferredDesc.myRenderPass     = aCore.myPostProcessState.myRhiHybridRenderPass;
    deferredDesc.mySampleCount    = 1;  // hybrid resolve color attachment is single-sample
    if ( !Gfx_DeferredLightingPass::CreatePipeline( rhi, deferredDesc, state.myDeferredGfx ) ) {
        return false;
    }

    const std::string         ddgiPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDdgiProbeUpdateSpv );
    const std::vector< char > ddgiSpv  = LoadSpirvBytes( ddgiPath );

    Gfx_DdgiPass::DestroyPipeline( rhi, state.myDdgiGfx );
    Gfx_DdgiPass::PipelineInitDesc ddgiDesc{};
    ddgiDesc.mySpirvCode  = ddgiSpv.data();
    ddgiDesc.mySpirvBytes = ddgiSpv.size();
    ddgiDesc.myLayout     = RhiVulkan::PipelineLayoutAdopt( state.myDdgiProbeUpdatePipelineLayout );
    if ( !Gfx_DdgiPass::CreatePipeline( rhi, ddgiDesc, state.myDdgiGfx ) ) {
        Gfx_DeferredLightingPass::DestroyPipeline( rhi, state.myDeferredGfx );
        return false;
    }

    SyncPipelineMirrors( aCore );
    return true;
}

void DestroyDdgiAtlas( Vk_Renderer& aCore ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( state.myDdgiProbeIrradianceAtlas.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( aCore.myRhi.myDeviceCtx.myDevice, state.myDdgiProbeIrradianceAtlas.ImageView(), nullptr );
        state.myDdgiProbeIrradianceAtlas.ImageView() = VK_NULL_HANDLE;
    }
    if ( state.myDdgiProbeIrradianceAtlas.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( aCore.myRhi.myDeviceCtx.myAllocator, state.myDdgiProbeIrradianceAtlas.Image(), state.myDdgiProbeIrradianceAtlas.Allocation() );
        state.myDdgiProbeIrradianceAtlas.AllocImage() = {};
    }
    if ( state.myDdgiProbeVisibilityAtlas.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( aCore.myRhi.myDeviceCtx.myDevice, state.myDdgiProbeVisibilityAtlas.ImageView(), nullptr );
        state.myDdgiProbeVisibilityAtlas.ImageView() = VK_NULL_HANDLE;
    }
    if ( state.myDdgiProbeVisibilityAtlas.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( aCore.myRhi.myDeviceCtx.myAllocator, state.myDdgiProbeVisibilityAtlas.Image(), state.myDdgiProbeVisibilityAtlas.Allocation() );
        state.myDdgiProbeVisibilityAtlas.AllocImage() = {};
    }
}

void CreateDdgiAtlas( Vk_Renderer& aCore ) {
    DestroyDdgiAtlas( aCore );
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    state.myDdgiProbeCountX         = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountX );
    state.myDdgiProbeCountY         = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountY );
    state.myDdgiProbeCountZ         = std::max( 1u, aCore.myLightingSettings.myDdgiProbeCountZ );
    state.myDdgiTotalProbeCount     = state.myDdgiProbeCountX * state.myDdgiProbeCountY * state.myDdgiProbeCountZ;

    const VkExtent2D atlasExtent{ state.myDdgiProbeCountX * state.myDdgiProbeCountY, state.myDdgiProbeCountZ };
    aCore.CreateImage( atlasExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                       1, VK_SAMPLE_COUNT_1_BIT, state.myDdgiProbeIrradianceAtlas.AllocImage() );
    state.myDdgiProbeIrradianceAtlas.ImageView() = aCore.CreateImageView( state.myDdgiProbeIrradianceAtlas.Image(), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );

    aCore.CreateImage( atlasExtent, VK_FORMAT_R16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, state.myDdgiProbeVisibilityAtlas.AllocImage() );
    state.myDdgiProbeVisibilityAtlas.ImageView() = aCore.CreateImageView( state.myDdgiProbeVisibilityAtlas.Image(), VK_FORMAT_R16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT );
    // Deferred always samples these as SHADER_READ_ONLY (even when DDGI is off) — leave UNDEFINED and validation fails.
    aCore.TransitionImageLayout( state.myDdgiProbeIrradianceAtlas.Image(), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 1 );
    aCore.TransitionImageLayout( state.myDdgiProbeVisibilityAtlas.Image(), VK_FORMAT_R16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    state.myDdgiAtlasReadable     = false;
    state.myDdgiHistoryForceReset = true;
    state.myDdgiPrevVolumeCenter  = aCore.myLightingSettings.myDdgiVolumeCenter;
    state.myDdgiPrevVolumeExtents = aCore.myLightingSettings.myDdgiVolumeExtents;
    state.myDdgiPrevCameraEye     = aCore.myPrimaryCamera.myEye;
}

// Per in-flight frame: cluster-list SSBO differs; G-buffer views refresh on resize.
void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;

    // Validation rejects null imageView on COMBINED_IMAGE_SAMPLER; use albedo when a feature target is absent.
    const VkImageView safeView = aCore.myGBufferState.myAlbedo.ImageView();
    auto              orSafe   = [ safeView ]( VkImageView aView ) { return aView != VK_NULL_HANDLE ? aView : safeView; };

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.sampler     = state.myGBufferSampler;
    albedoInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler     = state.myGBufferSampler;
    normalInfo.imageView   = aCore.myGBufferState.myNormalRoughness.ImageView();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo worldPosInfo{};
    worldPosInfo.sampler     = state.myGBufferSampler;
    worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
    worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightsInfo{};
    lightsInfo.buffer = aCore.myClusterBuildState.myLightsBuffer.myBuffer;
    lightsInfo.offset = 0;
    lightsInfo.range  = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;

    VkDescriptorBufferInfo listsInfo{};
    listsInfo.buffer = aCore.myClusterBuildState.myClusterListBuffers[ aFrameIndex ].myBuffer;
    listsInfo.offset = 0;
    listsInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightingGlobalsInfo{};
    lightingGlobalsInfo.buffer = aCore.myLightingGlobalsBuffer.myBuffer;
    lightingGlobalsInfo.offset = aCore.PadUniformBufferSize( sizeof( Gpu_LightingGlobals ) ) * aFrameIndex;
    lightingGlobalsInfo.range  = sizeof( Gpu_LightingGlobals );

    VkDescriptorImageInfo shadowInfo{};
    shadowInfo.sampler     = aCore.myShadowMapState.myCompareSampler;
    shadowInfo.imageView   = orSafe( aCore.myShadowMapState.myDepth.ImageView() );
    shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo irradianceInfo{};
    irradianceInfo.sampler     = aCore.myIblResourcesState.myCubemapSampler;
    irradianceInfo.imageView   = orSafe( aCore.myIblResourcesState.myIrradiance.ImageView() );
    irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo prefilterInfo{};
    prefilterInfo.sampler     = aCore.myIblResourcesState.myCubemapSampler;
    prefilterInfo.imageView   = orSafe( aCore.myIblResourcesState.myPrefilter.ImageView() );
    prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo brdfLutInfo{};
    brdfLutInfo.sampler     = aCore.myIblResourcesState.myBrdfLutSampler;
    brdfLutInfo.imageView   = orSafe( aCore.myIblResourcesState.myBrdfLut.ImageView() );
    brdfLutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo skyInfo{};
    skyInfo.sampler     = aCore.myIblResourcesState.myCubemapSampler;
    skyInfo.imageView   = orSafe( aCore.myIblResourcesState.mySky.ImageView() );
    skyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo shadowDepthReadInfo{};
    shadowDepthReadInfo.sampler     = aCore.myShadowMapState.myDepthReadSampler;
    shadowDepthReadInfo.imageView   = orSafe( aCore.myShadowMapState.myDepth.ImageView() );
    shadowDepthReadInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo aoInfo{};
    aoInfo.sampler     = state.myGBufferSampler;
    aoInfo.imageView   = orSafe( Vk_ShadowAoSoftPass::GetDeferredContactMapView( aCore ) );
    aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo hiZInfo{};
    hiZInfo.sampler     = aCore.myDepthPyramidState.myPyramidSampler;
    hiZInfo.imageView   = orSafe( aCore.myDepthPyramidState.myPyramid.ImageView() );
    hiZInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo ddgiProbeInfo{};
    ddgiProbeInfo.sampler     = state.myGBufferSampler;
    ddgiProbeInfo.imageView   = orSafe( state.myDdgiProbeIrradianceAtlas.ImageView() );
    ddgiProbeInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo ddgiVisibilityInfo{};
    ddgiVisibilityInfo.sampler     = state.myGBufferSampler;
    ddgiVisibilityInfo.imageView   = orSafe( state.myDdgiProbeVisibilityAtlas.ImageView() );
    ddgiVisibilityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo ssrInfo{};
    ssrInfo.sampler     = state.myGBufferSampler;
    ssrInfo.imageView   = orSafe( Vk_SsrPass::GetOutputImageView( aCore ) );
    ssrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo bentInfo{};
    bentInfo.sampler     = state.myGBufferSampler;
    bentInfo.imageView   = orSafe( Vk_AoPass::GetBentNormalHalfView( aCore ) );
    bentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo mvInfo{};
    mvInfo.sampler     = state.myGBufferSampler;
    mvInfo.imageView   = aCore.myGBufferState.myMotionVector.ImageView();
    mvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array< VkWriteDescriptorSet, 20 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightsInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &listsInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 4, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightingGlobalsInfo, 5, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowInfo, 6, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &irradianceInfo, 7, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &prefilterInfo, 8, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &brdfLutInfo, 9, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &skyInfo, 10, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowDepthReadInfo, 11, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 12, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &aoInfo, 13, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hiZInfo, 14, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ddgiProbeInfo, 15, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ddgiVisibilityInfo, 16, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ssrInfo, 17, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bentInfo, 18, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mvInfo, 19, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void CreatePipelineResources( Vk_Renderer& aCore ) {
    if ( !Vk_PostProcessPass::HasHybridResolve( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: PostProcess hybrid resolve render pass is required" );
    }

    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;

    const std::array< VkDescriptorSetLayoutBinding, 20 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 7 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 8 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 9 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 10 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 11 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 12 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 13 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 14 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 15 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 16 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 17 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 18 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 19 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.mySetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create descriptor set layout" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( Gpu_DeferredLightingPushConstants );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.mySetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create pipeline layout" );
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create sampler" );
    }

    std::array< VkDescriptorPoolSize, 3 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 20 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create descriptor pool" );
    }

    // Atlases must exist before UpdateDescriptorSet — bindings 15/16 reject null imageView.
    CreateDdgiAtlas( aCore );

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &state.mySetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_DeferredLightingPass: failed to allocate descriptor set" );
        }
        UpdateDescriptorSet( aCore, i );
    }

    const std::array< VkDescriptorSetLayoutBinding, 4 > ddgiBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
    };
    VkDescriptorSetLayoutCreateInfo ddgiLayoutInfo{};
    ddgiLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ddgiLayoutInfo.bindingCount = static_cast< uint32_t >( ddgiBindings.size() );
    ddgiLayoutInfo.pBindings    = ddgiBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &ddgiLayoutInfo, nullptr, &state.myDdgiProbeSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create DDGI probe set layout" );
    }

    VkPushConstantRange ddgiPushRange{};
    ddgiPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    // Must match DdgiProbeUpdate.comp / RecordDdgiProbeUpdate push (7 x vec4 = 112 bytes).
    ddgiPushRange.size = sizeof( uint32_t ) * 8 + sizeof( float ) * 20;

    VkPipelineLayoutCreateInfo ddgiPipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    ddgiPipelineLayoutInfo.setLayoutCount             = 1;
    ddgiPipelineLayoutInfo.pSetLayouts                = &state.myDdgiProbeSetLayout;
    ddgiPipelineLayoutInfo.pushConstantRangeCount     = 1;
    ddgiPipelineLayoutInfo.pPushConstantRanges        = &ddgiPushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &ddgiPipelineLayoutInfo, nullptr, &state.myDdgiProbeUpdatePipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create DDGI probe pipeline layout" );
    }

    std::array< VkDescriptorPoolSize, 2 > ddgiPoolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 2 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 2 },
    };
    VkDescriptorPoolCreateInfo ddgiPoolInfo{};
    ddgiPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ddgiPoolInfo.poolSizeCount = static_cast< uint32_t >( ddgiPoolSizes.size() );
    ddgiPoolInfo.pPoolSizes    = ddgiPoolSizes.data();
    ddgiPoolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &ddgiPoolInfo, nullptr, &state.myDdgiProbeDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create DDGI probe descriptor pool" );
    }

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocDdgi{};
        allocDdgi.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocDdgi.descriptorPool     = state.myDdgiProbeDescriptorPool;
        allocDdgi.descriptorSetCount = 1;
        allocDdgi.pSetLayouts        = &state.myDdgiProbeSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocDdgi, &state.myDdgiProbeDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_DeferredLightingPass: failed to allocate DDGI probe descriptor set" );
        }
        VkDescriptorImageInfo outProbe{};
        outProbe.imageView   = state.myDdgiProbeIrradianceAtlas.ImageView();
        outProbe.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outVisibility{};
        outVisibility.imageView   = state.myDdgiProbeVisibilityAtlas.ImageView();
        outVisibility.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo worldPosInfo{};
        worldPosInfo.sampler     = state.myGBufferSampler;
        worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
        worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo{};
        normalInfo.sampler                           = state.myGBufferSampler;
        normalInfo.imageView                         = aCore.myGBufferState.myNormalRoughness.ImageView();
        normalInfo.imageLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::array< VkWriteDescriptorSet, 4 > writes = {
            VkInit::DescriptorSetWriteCreateInfo( state.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outProbe, 0, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( state.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outVisibility, 1, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( state.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( state.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 3, 1 ),
        };
        vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
    }

    if ( !CreateGfxPipelines( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx pipeline create failed" );
    }

    // DDGI atlases are owned by DestroyDdgiAtlas (called from Destroy / RecreateForExtent), not this queue —
    // capturing atlas handles here would double-free after resize and previously leaked the irradiance atlas.
    const VkDevice              device                  = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipelineLayout      pipelineLayout          = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout               = state.mySetLayout;
    const VkDescriptorPool      descriptorPool          = state.myDescriptorPool;
    const VkSampler             sampler                 = state.myGBufferSampler;
    const VkPipelineLayout      ddgiProbePipelineLayout = state.myDdgiProbeUpdatePipelineLayout;
    const VkDescriptorSetLayout ddgiProbeSetLayout      = state.myDdgiProbeSetLayout;
    const VkDescriptorPool      ddgiProbeDescriptorPool = state.myDdgiProbeDescriptorPool;
    // DDGI compute pipeline is destroyed in Destroy(); do not capture it here (avoids double-free).
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction(
        [ device, pipelineLayout, setLayout, descriptorPool, sampler, ddgiProbePipelineLayout, ddgiProbeSetLayout, ddgiProbeDescriptorPool ]() {
            if ( pipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
            }
            if ( setLayout != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device, setLayout, nullptr );
            }
            if ( descriptorPool != VK_NULL_HANDLE ) {
                vkDestroyDescriptorPool( device, descriptorPool, nullptr );
            }
            if ( sampler != VK_NULL_HANDLE ) {
                vkDestroySampler( device, sampler, nullptr );
            }
            if ( ddgiProbePipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, ddgiProbePipelineLayout, nullptr );
            }
            if ( ddgiProbeSetLayout != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device, ddgiProbeSetLayout, nullptr );
            }
            if ( ddgiProbeDescriptorPool != VK_NULL_HANDLE ) {
                vkDestroyDescriptorPool( device, ddgiProbeDescriptorPool, nullptr );
            }
        } );

    UtilLogger::Info( "PIPELINE", "DeferredLighting graphics pipeline created." );
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
        Gfx_DeferredLightingPass::DestroyPipeline( aCore.myGfxRhiDevice, aCore.myDeferredLightingState.myDeferredGfx );
        Gfx_DdgiPass::DestroyPipeline( aCore.myGfxRhiDevice, aCore.myDeferredLightingState.myDdgiGfx );
    }
    DestroyDdgiAtlas( aCore );
    aCore.myDeferredLightingState.myDescriptorSets                = {};
    aCore.myDeferredLightingState.myDdgiProbeDescriptorSets       = {};
    aCore.myDeferredLightingState.myPipeline                      = VK_NULL_HANDLE;
    aCore.myDeferredLightingState.myDdgiProbeUpdatePipeline       = VK_NULL_HANDLE;
    aCore.myDeferredLightingState.myDdgiProbeUpdatePipelineLayout = VK_NULL_HANDLE;
    aCore.myDeferredLightingState.myDdgiProbeSetLayout            = VK_NULL_HANDLE;
    aCore.myDeferredLightingState.myDdgiProbeDescriptorPool       = VK_NULL_HANDLE;
    aCore.myDeferredLightingState.myDdgiAtlasReadable             = false;
    aCore.myDeferredLightingState.myInitialized                   = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }

    DestroyDdgiAtlas( aCore );
    CreateDdgiAtlas( aCore );
    aCore.myDeferredLightingState.myDdgiUpdateCursor = 0u;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateDescriptorSet( aCore, i );
        VkDescriptorImageInfo outProbe{};
        outProbe.imageView   = aCore.myDeferredLightingState.myDdgiProbeIrradianceAtlas.ImageView();
        outProbe.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outVisibility{};
        outVisibility.imageView   = aCore.myDeferredLightingState.myDdgiProbeVisibilityAtlas.ImageView();
        outVisibility.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo worldPosInfo{};
        worldPosInfo.sampler     = aCore.myDeferredLightingState.myGBufferSampler;
        worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
        worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo normalInfo{};
        normalInfo.sampler                           = aCore.myDeferredLightingState.myGBufferSampler;
        normalInfo.imageView                         = aCore.myGBufferState.myNormalRoughness.ImageView();
        normalInfo.imageLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        std::array< VkWriteDescriptorSet, 4 > writes = {
            VkInit::DescriptorSetWriteCreateInfo( aCore.myDeferredLightingState.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outProbe, 0, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( aCore.myDeferredLightingState.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outVisibility, 1, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( aCore.myDeferredLightingState.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2,
                                                  1 ),
            VkInit::DescriptorSetWriteCreateInfo( aCore.myDeferredLightingState.myDdgiProbeDescriptorSets[ i ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 3, 1 ),
        };
        vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
    }

    if ( !CreateGfxPipelines( aCore ) ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: Gfx pipeline recreate failed" );
    }
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
    UtilLogger::Info( "FG", "Vk_DeferredLightingPass::Init." );
    CreatePipelineResources( aCore );
    aCore.myDeferredLightingState.myInitialized = true;
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

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

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

    Gfx_DdgiPass::RecordProbeUpdate( frameCmd.Get(), gpu, input );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myIrradianceAtlas );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myVisibilityAtlas );

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
    gpu.myPipeline = RhiVulkan::PipelineAdopt( state.myPipeline );
    gpu.myLayout   = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.mySet      = RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ] );

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

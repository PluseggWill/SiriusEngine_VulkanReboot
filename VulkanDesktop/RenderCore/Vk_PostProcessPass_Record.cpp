// Module: Vk_PostProcessPass record path — thin facade over Gfx_PostProcessPass (Rhi).
#include "Vk_PostProcessPass.h"

#include "Vk_PostProcessPassInternal.h"

#include "../Gfx/Gfx_PostProcessPass.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <algorithm>
#include <array>

namespace {

Rhi_ImageLayout ToRhiLayout( VkImageLayout aLayout ) {
    switch ( aLayout ) {
    case VK_IMAGE_LAYOUT_GENERAL:
        return Rhi_ImageLayout::General;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return Rhi_ImageLayout::ShaderReadOnly;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return Rhi_ImageLayout::TransferSrc;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return Rhi_ImageLayout::TransferDst;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return Rhi_ImageLayout::ColorAttachment;
    case VK_IMAGE_LAYOUT_UNDEFINED:
    default:
        return Rhi_ImageLayout::Undefined;
    }
}

VkImageLayout ToVkLayout( Rhi_ImageLayout aLayout ) {
    switch ( aLayout ) {
    case Rhi_ImageLayout::General:
        return VK_IMAGE_LAYOUT_GENERAL;
    case Rhi_ImageLayout::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case Rhi_ImageLayout::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Rhi_ImageLayout::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Rhi_ImageLayout::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case Rhi_ImageLayout::Undefined:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkExtent2D BloomExtent( const VkExtent2D& aFullExtent ) {
    return { std::max( 1u, aFullExtent.width / 2 ), std::max( 1u, aFullExtent.height / 2 ) };
}

void UpdateTonemapDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = state.mySceneSampler;
    sceneInfo.imageView   = ( post.myTaaEnabled && state.myTaaResolved.ImageView() != VK_NULL_HANDLE ) ? state.myTaaResolved.ImageView() : state.mySceneColor.ImageView();
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo bloomInfo{};
    bloomInfo.sampler     = state.mySceneSampler;
    bloomInfo.imageView   = state.myBloomPing.ImageView();
    bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTonemapDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTonemapDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bloomInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateTaaResolveDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = state.mySceneSampler;
    sceneInfo.imageView   = state.mySceneColor.ImageView();
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const uint32_t        readIndex = 1u - state.myTaaHistoryWriteIndex;
    VkDescriptorImageInfo historyInfo{};
    historyInfo.sampler     = state.mySceneSampler;
    historyInfo.imageView   = state.myTaaHistory[ readIndex ].ImageView();
    historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo mvInfo{};
    mvInfo.sampler     = state.mySceneSampler;
    mvInfo.imageView   = aCore.myGBufferState.myMotionVector.ImageView();
    mvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = state.myTaaResolved.ImageView();
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &historyInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mvInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void RecordBloom( Vk_Renderer& aCore, Rhi_CommandList& aCmd, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myBloomEnabled ) {
        return;
    }

    const VkExtent2D bloomExtent = BloomExtent( aCore.mySwapchainCtx.mySwapChainExtent );

    Gfx_PostProcessPass::BloomGpu gpu{};
    gpu.myThresholdPipeline = RhiVulkan::PipelineAdopt( state.myBloomThresholdPipeline );
    gpu.myThresholdLayout   = RhiVulkan::PipelineLayoutAdopt( state.myBloomThresholdPipelineLayout );
    gpu.myThresholdSet      = RhiVulkan::DescriptorSetAdopt( state.myBloomThresholdDescriptorSets[ aFrameIndex ] );
    gpu.myBlurPipeline      = RhiVulkan::PipelineAdopt( state.myBloomBlurPipeline );
    gpu.myBlurLayout        = RhiVulkan::PipelineLayoutAdopt( state.myBloomBlurPipelineLayout );
    gpu.myBlurHorizSet      = RhiVulkan::DescriptorSetAdopt( state.myBloomBlurHorizDescriptorSets[ aFrameIndex ] );
    gpu.myBlurVertSet       = RhiVulkan::DescriptorSetAdopt( state.myBloomBlurVertDescriptorSets[ aFrameIndex ] );
    gpu.mySceneColor        = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySceneColor.Image(), state.mySceneColor.ImageView(), kPostSceneColorFormat, 1 );
    gpu.myBloomPing         = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myBloomPing.Image(), state.myBloomPing.ImageView(), kPostSceneColorFormat, 1 );
    gpu.myBloomPong         = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myBloomPong.Image(), state.myBloomPong.ImageView(), kPostSceneColorFormat, 1 );

    Rhi_ImageLayout sceneLayout = ToRhiLayout( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout pingLayout  = ToRhiLayout( Vk_PostProcessPassDetail::gBloomPingLayout );
    Rhi_ImageLayout pongLayout  = ToRhiLayout( Vk_PostProcessPassDetail::gBloomPongLayout );

    Gfx_PostProcessPass::BloomInput input{};
    input.myWidth       = bloomExtent.width;
    input.myHeight      = bloomExtent.height;
    input.myThreshold   = post.myBloomThreshold;
    input.mySceneLayout = &sceneLayout;
    input.myPingLayout  = &pingLayout;
    input.myPongLayout  = &pongLayout;

    Gfx_PostProcessPass::RecordBloom( aCmd, gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout = ToVkLayout( sceneLayout );
    Vk_PostProcessPassDetail::gBloomPingLayout  = ToVkLayout( pingLayout );
    Vk_PostProcessPassDetail::gBloomPongLayout  = ToVkLayout( pongLayout );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySceneColor );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myBloomPing );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myBloomPong );
}

void RecordTaaResolve( Vk_Renderer& aCore, Rhi_CommandList& aCmd, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myTaaEnabled ) {
        state.myTaaHistoryReady = false;
        return;
    }

    if ( !aCore.myTemporalState.myHistoryValid ) {
        state.myTaaHistoryReady = false;
    }

    const uint32_t writeIndex = state.myTaaHistoryWriteIndex;
    const uint32_t readIndex  = 1u - writeIndex;
    UpdateTaaResolveDescriptorSet( aCore, aFrameIndex );

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;

    Gfx_PostProcessPass::TaaGpu gpu{};
    gpu.myPipeline   = RhiVulkan::PipelineAdopt( state.myTaaResolvePipeline );
    gpu.myLayout     = RhiVulkan::PipelineLayoutAdopt( state.myTaaResolvePipelineLayout );
    gpu.mySet        = RhiVulkan::DescriptorSetAdopt( state.myTaaResolveDescriptorSets[ aFrameIndex ] );
    gpu.mySceneColor = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySceneColor.Image(), state.mySceneColor.ImageView(), kPostSceneColorFormat, 1 );
    gpu.myResolved   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myTaaResolved.Image(), state.myTaaResolved.ImageView(), kPostSceneColorFormat, 1 );
    gpu.myHistoryRead =
        RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myTaaHistory[ readIndex ].Image(), state.myTaaHistory[ readIndex ].ImageView(), kPostSceneColorFormat, 1 );
    gpu.myHistoryWrite =
        RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myTaaHistory[ writeIndex ].Image(), state.myTaaHistory[ writeIndex ].ImageView(), kPostSceneColorFormat, 1 );

    Rhi_ImageLayout sceneLayout     = ToRhiLayout( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout resolvedLayout  = ToRhiLayout( Vk_PostProcessPassDetail::gTaaResolvedLayout );
    Rhi_ImageLayout histReadLayout  = ToRhiLayout( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] );
    Rhi_ImageLayout histWriteLayout = ToRhiLayout( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] );

    Gfx_PostProcessPass::TaaInput input{};
    input.myWidth              = extent.width;
    input.myHeight             = extent.height;
    input.myPush.historyBlend  = post.myTaaBlend;
    input.myPush.historyValid  = ( aCore.myTemporalState.myHistoryValid && state.myTaaHistoryReady ) ? 1.0f : 0.0f;
    input.myPush.varianceGamma = post.myTaaVarianceGamma;
    input.myPush.sharpen       = post.myTaaSharpen;
    input.mySceneLayout        = &sceneLayout;
    input.myResolvedLayout     = &resolvedLayout;
    input.myHistoryReadLayout  = &histReadLayout;
    input.myHistoryWriteLayout = &histWriteLayout;

    Gfx_PostProcessPass::RecordTaa( aCmd, gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout                = ToVkLayout( sceneLayout );
    Vk_PostProcessPassDetail::gTaaResolvedLayout               = ToVkLayout( resolvedLayout );
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ]  = ToVkLayout( histReadLayout );
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = ToVkLayout( histWriteLayout );
    state.myTaaHistoryWriteIndex                               = 1u - state.myTaaHistoryWriteIndex;
    state.myTaaHistoryReady                                    = true;

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySceneColor );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myResolved );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myHistoryRead );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myHistoryWrite );
}

}  // namespace

namespace Vk_PostProcessPass {

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    RecordTaaResolve( aCore, cmd, aFrameIndex );
    RecordBloom( aCore, cmd, aFrameIndex );

    const Gfx_PostSettings& post   = aCore.myPostSettings;
    const VkExtent2D        extent = aCore.mySwapchainCtx.mySwapChainExtent;

    UpdateTonemapDescriptorSet( aCore, aFrameIndex );

    Gfx_PostProcessPass::TonemapGpu gpu{};
    gpu.myPipeline    = RhiVulkan::PipelineAdopt( state.myTonemapPipeline );
    gpu.myLayout      = RhiVulkan::PipelineLayoutAdopt( state.myTonemapPipelineLayout );
    gpu.mySet         = RhiVulkan::DescriptorSetAdopt( state.myTonemapDescriptorSets[ aFrameIndex ] );
    gpu.myRenderPass  = RhiVulkan::RenderPassAdopt( aCore.mySwapchainCtx.myRenderPass );
    gpu.myFramebuffer = RhiVulkan::FramebufferAdopt( aCore.mySwapchainCtx.mySwapChainFrameBuffers[ aImageIndex ] );
    gpu.mySceneColor  = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySceneColor.Image(), state.mySceneColor.ImageView(), kPostSceneColorFormat, 1 );
    gpu.myBloomPing   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.myBloomPing.Image(), state.myBloomPing.ImageView(), kPostSceneColorFormat, 1 );

    Rhi_ImageLayout sceneLayout = ToRhiLayout( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout pingLayout  = ToRhiLayout( Vk_PostProcessPassDetail::gBloomPingLayout );

    Gfx_PostProcessPass::TonemapInput input{};
    input.myWidth               = extent.width;
    input.myHeight              = extent.height;
    input.myDebugLabels         = aCore.AreCommandDebugLabelsEnabled();
    input.myBloomEnabled        = post.myBloomEnabled;
    input.mySceneLayout         = &sceneLayout;
    input.myPingLayout          = &pingLayout;
    input.myPush.exposure       = post.myExposure;
    input.myPush.bloomIntensity = post.myBloomIntensity;
    input.myPush.tonemapEnabled = post.myTonemapEnabled ? 1u : 0u;
    input.myPush.bloomEnabled   = post.myBloomEnabled ? 1u : 0u;
    input.myPush.tonemapMode    = post.myTonemapMode != 0 ? 1u : 0u;

    Gfx_PostProcessPass::RecordTonemap( cmd, gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout = ToVkLayout( sceneLayout );
    Vk_PostProcessPassDetail::gBloomPingLayout  = ToVkLayout( pingLayout );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySceneColor );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myBloomPing );
    Rhi::CommandListDestroy( cmd );
}

}  // namespace Vk_PostProcessPass

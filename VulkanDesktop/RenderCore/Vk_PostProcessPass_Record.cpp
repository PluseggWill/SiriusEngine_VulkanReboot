// Module: Vk_PostProcessPass record path (TAA resolve, bloom, tonemap to swapchain).
#include "Vk_PostProcessPass.h"

#include "Vk_PostProcessPassInternal.h"

#include "../Gfx/Gfx_PostSettings.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <algorithm>
#include <array>
#include <string>

namespace {

struct TonemapPushConstants {
    float    exposure;
    float    bloomIntensity;
    uint32_t tonemapEnabled;
    uint32_t bloomEnabled;
    uint32_t tonemapMode;
};

static_assert( sizeof( TonemapPushConstants ) == 20, "TonemapPushConstants must match Tonemap.frag push block" );

struct BloomThresholdPushConstants {
    float threshold;
    float pad0;
    float pad1;
    float pad2;
};

static_assert( sizeof( BloomThresholdPushConstants ) == 16, "BloomThresholdPushConstants must match BloomThreshold.comp push block" );

struct BloomBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( BloomBlurPushConstants ) == 8, "BloomBlurPushConstants must match BloomBlur.comp push block" );

struct TaaResolvePushConstants {
    float historyBlend;
    float historyValid;
    float varianceGamma;
    float sharpen;
};
static_assert( sizeof( TaaResolvePushConstants ) == 16, "TaaResolvePushConstants must match TaaResolve.comp push block" );

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

VkImageMemoryBarrier SceneColorBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return barrier;
}

void RecordBloom( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;

    if ( !post.myBloomEnabled ) {
        return;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gBloomPingLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gBloomPingLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else if ( oldLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myBloomPing.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    if ( Vk_PostProcessPassDetail::gBloomPongLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gBloomPongLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else if ( oldLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myBloomPong.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPongLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    BloomThresholdPushConstants thresholdPush{ post.myBloomThreshold, 0.f, 0.f, 0.f };
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomThresholdPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomThresholdPipelineLayout, 0, 1, &state.myBloomThresholdDescriptorSets[ aFrameIndex ],
                             0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myBloomThresholdPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( thresholdPush ), &thresholdPush );

    const VkExtent2D bloomExtent = BloomExtent( aCore.mySwapchainCtx.mySwapChainExtent );
    vkCmdDispatch( aCommandBuffer, ( bloomExtent.width + 7 ) / 8, ( bloomExtent.height + 7 ) / 8, 1 );

    auto runBlurPass = [ & ]( bool aHorizontal ) {
        BloomBlurPushConstants blurPush{ aHorizontal ? 1u : 0u, aHorizontal ? 0u : 1u };
        VkDescriptorSet        set = aHorizontal ? state.myBloomBlurHorizDescriptorSets[ aFrameIndex ] : state.myBloomBlurVertDescriptorSets[ aFrameIndex ];
        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomBlurPipeline );
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomBlurPipelineLayout, 0, 1, &set, 0, nullptr );
        vkCmdPushConstants( aCommandBuffer, state.myBloomBlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( blurPush ), &blurPush );
        vkCmdDispatch( aCommandBuffer, ( bloomExtent.width + 7 ) / 8, ( bloomExtent.height + 7 ) / 8, 1 );
    };

    runBlurPass( true );

    VkMemoryBarrier blurDep{};
    blurDep.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blurDep.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    blurDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &blurDep, 0, nullptr, 0, nullptr );

    runBlurPass( false );

    // Two-pass blur ends in ping; tonemap samples bloom ping.
    VkImageMemoryBarrier bloomBarrier = SceneColorBarrier( state.myBloomPing.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bloomBarrier );
    Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void RecordTaaResolve( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myTaaEnabled ) {
        state.myTaaHistoryReady = false;
        return;
    }

    if ( !aCore.myTemporalState.myHistoryValid ) {
        state.myTaaHistoryReady = false;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    const uint32_t writeIndex = state.myTaaHistoryWriteIndex;
    const uint32_t readIndex  = 1u - writeIndex;

    if ( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ];
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_GENERAL ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier =
            SceneColorBarrier( state.myTaaHistory[ readIndex ].Image(), oldLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, srcAccess, VK_ACCESS_SHADER_READ_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gTaaResolvedLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaResolvedLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if ( oldLayout == VK_IMAGE_LAYOUT_GENERAL ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myTaaResolved.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaResolvedLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    TaaResolvePushConstants push{};
    push.historyBlend  = post.myTaaBlend;
    push.historyValid  = ( aCore.myTemporalState.myHistoryValid && state.myTaaHistoryReady ) ? 1.0f : 0.0f;
    push.varianceGamma = post.myTaaVarianceGamma;
    push.sharpen       = post.myTaaSharpen;

    UpdateTaaResolveDescriptorSet( aCore, aFrameIndex );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTaaResolvePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTaaResolvePipelineLayout, 0, 1, &state.myTaaResolveDescriptorSets[ aFrameIndex ], 0,
                             nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myTaaResolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    vkCmdDispatch( aCommandBuffer, ( extent.width + 7 ) / 8, ( extent.height + 7 ) / 8, 1 );

    VkMemoryBarrier dep{};
    dep.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    dep.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &dep, 0,
                          nullptr, 0, nullptr );

    VkImageMemoryBarrier resolvedToRead0 = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &resolvedToRead0 );
    Vk_PostProcessPassDetail::gTaaResolvedLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Copy resolved -> history (writeIndex) for next frame.
    if ( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ];
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier =
            SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount     = 1;
    copy.dstSubresource                = copy.srcSubresource;
    copy.extent                        = { extent.width, extent.height, 1 };
    VkImageMemoryBarrier resolvedToSrc = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT );
    VkImageMemoryBarrier historyToDst  = SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
    std::array< VkImageMemoryBarrier, 2 > toCopy = { resolvedToSrc, historyToDst };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toCopy.size() ), toCopy.data() );

    vkCmdCopyImage( aCommandBuffer, state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.myTaaHistory[ writeIndex ].Image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );

    VkImageMemoryBarrier resolvedToRead = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                             VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
    VkImageMemoryBarrier historyToRead  = SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    std::array< VkImageMemoryBarrier, 2 > toReadBarriers = { resolvedToRead, historyToRead };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toReadBarriers.size() ), toReadBarriers.data() );

    Vk_PostProcessPassDetail::gTaaResolvedLayout               = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    state.myTaaHistoryWriteIndex                               = 1u - state.myTaaHistoryWriteIndex;
    state.myTaaHistoryReady                                    = true;
}

}  // namespace

namespace Vk_PostProcessPass {

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
        return;
    }

    RecordTaaResolve( aCore, aCommandBuffer, aFrameIndex );
    RecordBloom( aCore, aCommandBuffer, aFrameIndex );

    const Gfx_PostSettings& post = aCore.myPostSettings;
    if ( !post.myBloomEnabled && Vk_PostProcessPassDetail::gBloomPingLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        const VkImageLayout        oldLayout = Vk_PostProcessPassDetail::gBloomPingLayout;
        VkImageMemoryBarrier       barrier = SceneColorBarrier( state.myBloomPing.Image(), oldLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT );
        const VkPipelineStageFlags srcStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        // Hybrid RP finalLayout is already SHADER_READ_ONLY; Prefer MarkSceneColorShaderRead after resolve.
        // Avoid UNDEFINED→READ (discards contents). Only barrier when we know a real prior layout.
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Refresh every frame so ImGui TAA toggle cannot leave tonemap bound to an unwritten taaResolved.
    UpdateTonemapDescriptorSet( aCore, aFrameIndex );

    VkRenderPassBeginInfo tonemapBegin{};
    tonemapBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    tonemapBegin.renderPass        = aCore.mySwapchainCtx.myRenderPass;
    tonemapBegin.framebuffer       = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ aImageIndex ];
    tonemapBegin.renderArea.offset = { 0, 0 };
    tonemapBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 2 > clears{};
    clears[ 0 ].color            = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clears[ 1 ].depthStencil     = { 1.0f, 0 };
    tonemapBegin.clearValueCount = static_cast< uint32_t >( clears.size() );
    tonemapBegin.pClearValues    = clears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &tonemapBegin, VK_SUBPASS_CONTENTS_INLINE );

    TonemapPushConstants push{};
    push.exposure       = post.myExposure;
    push.bloomIntensity = post.myBloomIntensity;
    push.tonemapEnabled = post.myTonemapEnabled ? 1u : 0u;
    push.bloomEnabled   = post.myBloomEnabled ? 1u : 0u;
    push.tonemapMode    = post.myTonemapMode != 0 ? 1u : 0u;

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myTonemapPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myTonemapPipelineLayout, 0, 1, &state.myTonemapDescriptorSets[ aFrameIndex ], 0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myTonemapPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Tonemap" );
    }
    vkCmdDraw( aCommandBuffer, 3, 1, 0, 0 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

}  // namespace Vk_PostProcessPass

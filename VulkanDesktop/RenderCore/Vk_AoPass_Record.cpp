// Module: Vk_AoPass record path (SSAO / HBAO+ / GTAO + temporal).
#include "Vk_AoPass.h"

#include "Vk_AoPassInternal.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Util/Util_Logger.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace {

struct ClassicAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 viewProj;
    alignas( 16 ) glm::vec4 params;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( ClassicAoPushConstants ) == 224, "ClassicAoPushConstants must match Ssao.comp push block" );

// Shared by HbaoPlus.comp and Gtao.comp (same bindings; params.xy/z/w meaning differs per shader).
struct HalfResAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::vec4 params;  // HBAO: radius,bias,enabled,0 — GTAO: radius,0,enabled,falloff
    alignas( 8 ) glm::uvec2 sliceCount;
    alignas( 8 ) glm::uvec2 stepsPerSlice;
    alignas( 8 ) glm::vec2 halfScreenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( HalfResAoPushConstants ) == 176, "HalfResAoPushConstants must match half-res AO compute shaders" );

struct AoUpsamplePushConstants {
    alignas( 8 ) glm::vec2 fullScreenSize;
    float depthSigma;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoUpsamplePushConstants ) == 24, "AoUpsamplePushConstants must match AoUpsample.comp push block" );

struct AoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( AoBlurPushConstants ) == 8, "AoBlurPushConstants must match SsaoBlur.comp push block" );

struct AoTemporalPushConstants {
    alignas( 4 ) float historyBlend;
    alignas( 4 ) float historyValid;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoTemporalPushConstants ) == 16, "AoTemporalPushConstants must match AoTemporal.comp push block" );

VkExtent2D HalfExtent( uint32_t aWidth, uint32_t aHeight ) {
    return { std::max( 1u, ( aWidth + 1u ) / 2u ), std::max( 1u, ( aHeight + 1u ) / 2u ) };
}

VkImageMemoryBarrier ColorImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
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

void UpdateTemporalDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;

    const uint32_t readIndex  = state.myTemporalReadIndex % 2u;
    const uint32_t writeIndex = ( readIndex + 1u ) % 2u;

    VkDescriptorImageInfo mvInfo{};
    mvInfo.sampler     = state.myGBufferSampler;
    mvInfo.imageView   = aCore.myGBufferState.myMotionVector.ImageView();
    mvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo currentAoInfo{};
    currentAoInfo.sampler     = state.myGBufferSampler;
    currentAoInfo.imageView   = state.myAoRaw.ImageView();
    currentAoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo historyInfo{};
    historyInfo.sampler     = state.myGBufferSampler;
    historyInfo.imageView   = state.myAoHistory[ readIndex ].ImageView();
    historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = state.myAoHistory[ writeIndex ].ImageView();
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mvInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &currentAoInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &historyInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void CmdBarrierGBufferForAoRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    std::array< VkImageMemoryBarrier, 2 > barriers = {
        ColorImageBarrier( aCore.myGBufferState.myNormalRoughness.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myWorldPosition.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( barriers.size() ), barriers.data() );
}

void CmdBarrierAoForComputeWrite( VkCommandBuffer aCommandBuffer, VkImage aImage, VkImageLayout& aInOutLayout ) {
    const VkImageLayout  oldLayout = aInOutLayout;
    const VkImageLayout  newLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageMemoryBarrier barrier =
        ColorImageBarrier( aImage, oldLayout, newLayout, oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT );
    const VkPipelineStageFlags srcStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED                  ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                          : oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                                                                  : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    aInOutLayout = newLayout;
}

void CmdDispatchBlur( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, VkDescriptorSet aSet, uint32_t aAxisX, uint32_t aAxisY, uint32_t aWidth, uint32_t aHeight,
                      const char* aDebugLabel ) {
    Vk_AoState& state = aCore.myAoState;
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBlurPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBlurPipelineLayout, 0, 1, &aSet, 0, nullptr );

    AoBlurPushConstants push{};
    push.axisX = aAxisX;
    push.axisY = aAxisY;
    vkCmdPushConstants( aCommandBuffer, state.myBlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, aDebugLabel );
    }
    vkCmdDispatch( aCommandBuffer, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

void RecordClassicSsao( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight, const Gfx_AoSettings& ao ) {
    Vk_AoState& state = aCore.myAoState;

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myClassicPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myClassicPipelineLayout, 0, 1, &state.myClassicDescriptorSets[ aFrameIndex ], 0, nullptr );

    ClassicAoPushConstants push{};
    push.view       = aCore.myPrimaryCamera.myView;
    push.proj       = aCore.myPrimaryCamera.myProj;
    push.viewProj   = aCore.myPrimaryCamera.myProj * aCore.myPrimaryCamera.myView;
    push.params     = glm::vec4( ao.myRadius, ao.myBias, ao.myNormalAwareRadius, ao.myEnabled ? 1.0f : 0.0f );
    push.screenSize = glm::vec2( static_cast< float >( aWidth ), static_cast< float >( aHeight ) );
    vkCmdPushConstants( aCommandBuffer, state.myClassicPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=AO ClassicSSAO" );
    }
    vkCmdDispatch( aCommandBuffer, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

// Half-res compute → myAoHalf (SHADER_READ_ONLY) → AoUpsample.comp → myAoRaw.
void RecordHalfResUpsample( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight, const Gfx_AoSettings& ao ) {
    Vk_AoState& state = aCore.myAoState;

    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoRaw.Image(), Vk_AoPassDetail::gAoRawLayout );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myUpsamplePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myUpsamplePipelineLayout, 0, 1, &state.myUpsampleDescriptorSets[ aFrameIndex ], 0,
                             nullptr );

    AoUpsamplePushConstants upPush{};
    upPush.fullScreenSize = glm::vec2( static_cast< float >( aWidth ), static_cast< float >( aHeight ) );
    upPush.depthSigma     = ao.myUpsampleDepthSigma;
    vkCmdPushConstants( aCommandBuffer, state.myUpsamplePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( upPush ), &upPush );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=AO Upsample" );
    }
    vkCmdDispatch( aCommandBuffer, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

// Dispatch one half-res AO shader (HBAO+ or GTAO) into myAoHalf (and optional bent normal RG8).
void RecordHalfResCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight, VkPipeline aPipeline,
                           const HalfResAoPushConstants& aPush, const char* aDebugLabel, bool aWritesBentNormal ) {
    Vk_AoState&      state = aCore.myAoState;
    const VkExtent2D half  = HalfExtent( aWidth, aHeight );

    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoHalf.Image(), Vk_AoPassDetail::gAoHalfLayout );
    if ( aWritesBentNormal ) {
        CmdBarrierAoForComputeWrite( aCommandBuffer, state.myBentNormalHalf.Image(), Vk_AoPassDetail::gBentHalfLayout );
    }

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, aPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myHalfResPipelineLayout, 0, 1, &state.myHalfResDescriptorSets[ aFrameIndex ], 0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myHalfResPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( aPush ), &aPush );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, aDebugLabel );
    }
    vkCmdDispatch( aCommandBuffer, ( half.width + 7 ) / 8, ( half.height + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    VkImageMemoryBarrier halfWritten =
        ColorImageBarrier( state.myAoHalf.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &halfWritten );
    Vk_AoPassDetail::gAoHalfLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if ( aWritesBentNormal ) {
        VkImageMemoryBarrier bentWritten = ColorImageBarrier( state.myBentNormalHalf.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bentWritten );
        Vk_AoPassDetail::gBentHalfLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

void RecordHbaoPlus( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight, const Gfx_AoSettings& ao ) {
    Vk_AoState&      state = aCore.myAoState;
    const VkExtent2D half  = HalfExtent( aWidth, aHeight );

    HalfResAoPushConstants push{};
    push.view           = aCore.myPrimaryCamera.myView;
    push.proj           = aCore.myPrimaryCamera.myProj;
    push.params         = glm::vec4( ao.myRadius, ao.myBias, ao.myEnabled ? 1.0f : 0.0f, ao.myNormalAwareRadius );
    push.sliceCount     = glm::uvec2( std::clamp( ao.myHbaoDirections, 1u, 8u ), 0u );
    push.stepsPerSlice  = glm::uvec2( std::clamp( ao.myHbaoSteps, 1u, 8u ), 0u );
    push.halfScreenSize = glm::vec2( static_cast< float >( half.width ), static_cast< float >( half.height ) );

    RecordHalfResCompute( aCore, aCommandBuffer, aFrameIndex, aWidth, aHeight, state.myHbaoPipeline, push, "Pass=AO HBAO+", false );
    RecordHalfResUpsample( aCore, aCommandBuffer, aFrameIndex, aWidth, aHeight, ao );
}

void RecordGtao( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight, const Gfx_AoSettings& ao ) {
    // params: radius, unused, enabled, distance falloff — see Gtao.comp
    Vk_AoState&      state = aCore.myAoState;
    const VkExtent2D half  = HalfExtent( aWidth, aHeight );

    HalfResAoPushConstants push{};
    push.view           = aCore.myPrimaryCamera.myView;
    push.proj           = aCore.myPrimaryCamera.myProj;
    push.params         = glm::vec4( ao.myRadius, ao.myNormalAwareRadius, ao.myEnabled ? 1.0f : 0.0f, ao.myGtaoFalloff );
    push.sliceCount     = glm::uvec2( std::clamp( ao.myGtaoSlices, 1u, 16u ), 0u );
    push.stepsPerSlice  = glm::uvec2( std::clamp( ao.myGtaoStepsPerSlice, 1u, 16u ), 0u );
    push.halfScreenSize = glm::vec2( static_cast< float >( half.width ), static_cast< float >( half.height ) );

    RecordHalfResCompute( aCore, aCommandBuffer, aFrameIndex, aWidth, aHeight, state.myGtaoPipeline, push, "Pass=AO GTAO", true );
    RecordHalfResUpsample( aCore, aCommandBuffer, aFrameIndex, aWidth, aHeight, ao );
}

void RecordClassicBlur( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, uint32_t aWidth, uint32_t aHeight ) {
    Vk_AoState& state = aCore.myAoState;

    VkImageMemoryBarrier rawWritten =
        ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rawWritten );

    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoBlur.Image(), Vk_AoPassDetail::gAoBlurLayout );

    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurHorizDescriptorSets[ aFrameIndex ], 1, 0, aWidth, aHeight, "Pass=AO BlurH" );

    VkImageMemoryBarrier blurWritten =
        ColorImageBarrier( state.myAoBlur.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &blurWritten );

    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurVertDescriptorSets[ aFrameIndex ], 0, 1, aWidth, aHeight, "Pass=AO BlurV" );
}

void RecordTemporalFilter( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, const Gfx_AoSettings& ao ) {
    Vk_AoState& state = aCore.myAoState;
    if ( !ao.myTemporalEnabled ) {
        state.myTemporalHistoryReady = false;
        return;
    }

    UpdateTemporalDescriptorSet( aCore, aFrameIndex );

    const uint32_t readIndex  = state.myTemporalReadIndex % 2u;
    const uint32_t writeIndex = ( readIndex + 1u ) % 2u;

    VkImageMemoryBarrier currentAoReadable =
        ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &currentAoReadable );
    Vk_AoPassDetail::gAoRawLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const VkImageLayout        historyReadOld      = state.myTemporalHistoryReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkPipelineStageFlags historyReadSrcStage = state.myTemporalHistoryReady ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkImageMemoryBarrier       historyReadBarrier  = ColorImageBarrier( state.myAoHistory[ readIndex ].Image(), historyReadOld, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                 state.myTemporalHistoryReady ? VK_ACCESS_SHADER_WRITE_BIT : 0, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, historyReadSrcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &historyReadBarrier );

    const VkImageLayout        historyWriteOld      = state.myTemporalHistoryReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    const VkPipelineStageFlags historyWriteSrcStage = state.myTemporalHistoryReady ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkImageMemoryBarrier       historyWriteGeneral =
        ColorImageBarrier( state.myAoHistory[ writeIndex ].Image(), historyWriteOld, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, historyWriteSrcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &historyWriteGeneral );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTemporalPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTemporalPipelineLayout, 0, 1, &state.myTemporalDescriptorSets[ aFrameIndex ], 0,
                             nullptr );

    AoTemporalPushConstants push{};
    push.historyBlend = std::clamp( ao.myTemporalBlend, 0.0f, 0.98f );
    // Shared camera contract AND pass-local history buffer ready (same pattern as TAA).
    push.historyValid = ( aCore.myTemporalState.myHistoryValid && state.myTemporalHistoryReady ) ? 1.0f : 0.0f;
    vkCmdPushConstants( aCommandBuffer, state.myTemporalPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=AO Temporal" );
    }
    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    vkCmdDispatch( aCommandBuffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    VkImageMemoryBarrier historyWriteReadable    = ColorImageBarrier( state.myAoHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                                      VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );
    VkImageMemoryBarrier rawTransferDst          = ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                                      VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
    std::array< VkImageMemoryBarrier, 2 > toCopy = { historyWriteReadable, rawTransferDst };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( toCopy.size() ), toCopy.data() );

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount = 1;
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.layerCount = 1;
    copy.extent                    = { width, height, 1 };
    vkCmdCopyImage( aCommandBuffer, state.myAoHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.myAoRaw.Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copy );

    VkImageMemoryBarrier rawReadForDeferred      = ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    VkImageMemoryBarrier historyReadForNext      = ColorImageBarrier( state.myAoHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
    std::array< VkImageMemoryBarrier, 2 > toRead = { rawReadForDeferred, historyReadForNext };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toRead.size() ), toRead.data() );

    state.myTemporalReadIndex     = writeIndex;
    state.myTemporalHistoryReady  = true;
    Vk_AoPassDetail::gAoRawLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void TransitionRawForDeferred( VkCommandBuffer aCommandBuffer, VkImage aRawImage ) {
    VkImageMemoryBarrier aoForDeferred =
        ColorImageBarrier( aRawImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &aoForDeferred );
    Vk_AoPassDetail::gAoRawLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

}  // namespace

namespace Vk_AoPass {

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    const Gfx_AoSettings& ao = aCore.myAoSettings;
    CmdBarrierGBufferForAoRead( aCore, aCommandBuffer );
    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoRaw.Image(), Vk_AoPassDetail::gAoRawLayout );

    if ( ao.myMethod == Gfx_AoMethod::HbaoPlus ) {
        RecordHbaoPlus( aCore, aCommandBuffer, aFrameIndex, width, height, ao );
    }
    else if ( ao.myMethod == Gfx_AoMethod::Gtao ) {
        RecordGtao( aCore, aCommandBuffer, aFrameIndex, width, height, ao );
    }
    else {
        RecordClassicSsao( aCore, aCommandBuffer, aFrameIndex, width, height, ao );
    }

    RecordTemporalFilter( aCore, aCommandBuffer, aFrameIndex, ao );

    const bool contactSoft = ao.myContactSoftEnabled && aCore.myShadowAoSoftState.myInitialized;
    if ( contactSoft ) {
        // Raw AO may be SHADER_READ_ONLY after prior frame; ShadowAoSoft pack reads via sampler.
        return;
    }

    if ( ao.myMethod == Gfx_AoMethod::ClassicSsao ) {
        RecordClassicBlur( aCore, aCommandBuffer, aFrameIndex, width, height );
    }

    if ( !ao.myTemporalEnabled ) {
        TransitionRawForDeferred( aCommandBuffer, state.myAoRaw.Image() );
    }
}

}  // namespace Vk_AoPass

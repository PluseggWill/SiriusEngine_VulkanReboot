// Module: Vk_AoPass record path — thin facade over Gfx_AoPass::Record (Rhi).
#include "Vk_AoPass.h"

#include "Vk_AoPassInternal.h"

#include "../Gfx/Gfx_AoPass.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

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
    case Rhi_ImageLayout::Undefined:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
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

void PrepareTemporalDescriptorsThunk( void* aUser, uint32_t aFrameIndex ) {
    UpdateTemporalDescriptorSet( *static_cast< Vk_Renderer* >( aUser ), aFrameIndex );
}

Gfx_AoPass::GpuResources BuildAoGpuResources( Vk_Renderer& aCore, Rhi_Device& aRhiDevice, uint32_t aFrameIndex ) {
    Vk_AoState&              state = aCore.myAoState;
    Gfx_AoPass::GpuResources gpu{};

    gpu.myClassicPipeline  = RhiVulkan::PipelineAdopt( state.myClassicPipeline );
    gpu.myHbaoPipeline     = RhiVulkan::PipelineAdopt( state.myHbaoPipeline );
    gpu.myGtaoPipeline     = RhiVulkan::PipelineAdopt( state.myGtaoPipeline );
    gpu.myUpsamplePipeline = RhiVulkan::PipelineAdopt( state.myUpsamplePipeline );
    gpu.myBlurPipeline     = RhiVulkan::PipelineAdopt( state.myBlurPipeline );
    gpu.myTemporalPipeline = RhiVulkan::PipelineAdopt( state.myTemporalPipeline );

    gpu.myClassicLayout  = RhiVulkan::PipelineLayoutAdopt( state.myClassicPipelineLayout );
    gpu.myHalfResLayout  = RhiVulkan::PipelineLayoutAdopt( state.myHalfResPipelineLayout );
    gpu.myUpsampleLayout = RhiVulkan::PipelineLayoutAdopt( state.myUpsamplePipelineLayout );
    gpu.myBlurLayout     = RhiVulkan::PipelineLayoutAdopt( state.myBlurPipelineLayout );
    gpu.myTemporalLayout = RhiVulkan::PipelineLayoutAdopt( state.myTemporalPipelineLayout );

    gpu.myClassicSet   = RhiVulkan::DescriptorSetAdopt( state.myClassicDescriptorSets[ aFrameIndex ] );
    gpu.myHalfResSet   = RhiVulkan::DescriptorSetAdopt( state.myHalfResDescriptorSets[ aFrameIndex ] );
    gpu.myUpsampleSet  = RhiVulkan::DescriptorSetAdopt( state.myUpsampleDescriptorSets[ aFrameIndex ] );
    gpu.myBlurHorizSet = RhiVulkan::DescriptorSetAdopt( state.myBlurHorizDescriptorSets[ aFrameIndex ] );
    gpu.myBlurVertSet  = RhiVulkan::DescriptorSetAdopt( state.myBlurVertDescriptorSets[ aFrameIndex ] );
    gpu.myTemporalSet  = RhiVulkan::DescriptorSetAdopt( state.myTemporalDescriptorSets[ aFrameIndex ] );

    gpu.myAoRaw           = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoRaw.Image(), state.myAoRaw.ImageView(), VK_FORMAT_R8_UNORM, 1 );
    gpu.myAoHalf          = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHalf.Image(), state.myAoHalf.ImageView(), VK_FORMAT_R8_UNORM, 1 );
    gpu.myBentNormalHalf  = RhiVulkan::TextureAdopt( aRhiDevice, state.myBentNormalHalf.Image(), state.myBentNormalHalf.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
    gpu.myAoBlur          = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoBlur.Image(), state.myAoBlur.ImageView(), VK_FORMAT_R8_UNORM, 1 );
    gpu.myAoHistory0      = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHistory[ 0 ].Image(), state.myAoHistory[ 0 ].ImageView(), VK_FORMAT_R8_UNORM, 1 );
    gpu.myAoHistory1      = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHistory[ 1 ].Image(), state.myAoHistory[ 1 ].ImageView(), VK_FORMAT_R8_UNORM, 1 );
    gpu.myGBufferNormal   = RhiVulkan::TextureAdopt( aRhiDevice, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    return gpu;
}

void ReleaseAdoptedTextures( Rhi_Device& aRhiDevice, Gfx_AoPass::GpuResources& aGpu ) {
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoRaw );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHalf );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myBentNormalHalf );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoBlur );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHistory0 );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHistory1 );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferNormal );
    Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferWorldPos );
}

}  // namespace

namespace Vk_AoPass {

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_AoPass::GpuResources gpu = BuildAoGpuResources( aCore, aCore.myGfxRhiDevice, aFrameIndex );

    Gfx_AoPass::LayoutState layouts{};
    layouts.myAoRaw    = ToRhiLayout( Vk_AoPassDetail::gAoRawLayout );
    layouts.myAoHalf   = ToRhiLayout( Vk_AoPassDetail::gAoHalfLayout );
    layouts.myBentHalf = ToRhiLayout( Vk_AoPassDetail::gBentHalfLayout );
    layouts.myAoBlur   = ToRhiLayout( Vk_AoPassDetail::gAoBlurLayout );

    Gfx_AoPass::RecordInput input{};
    input.mySettings                   = aCore.myAoSettings;
    input.myView                       = aCore.myPrimaryCamera.myView;
    input.myProj                       = aCore.myPrimaryCamera.myProj;
    input.myWidth                      = width;
    input.myHeight                     = height;
    input.myFrameIndex                 = aFrameIndex;
    input.myDebugLabels                = aCore.AreCommandDebugLabelsEnabled();
    input.myContactSoftActive          = aCore.myAoSettings.myContactSoftEnabled && aCore.myShadowAoSoftState.myInitialized;
    input.mySharedTemporalHistoryValid = aCore.myTemporalState.myHistoryValid;
    input.myTemporalReadIndex          = &state.myTemporalReadIndex;
    input.myTemporalHistoryReady       = &state.myTemporalHistoryReady;
    input.myLayouts                    = &layouts;
    input.myPrepareTemporalDescriptors = &PrepareTemporalDescriptorsThunk;
    input.myPrepareTemporalUser        = &aCore;

    Gfx_AoPass::Record( cmd, gpu, input );

    Vk_AoPassDetail::gAoRawLayout    = ToVkLayout( layouts.myAoRaw );
    Vk_AoPassDetail::gAoHalfLayout   = ToVkLayout( layouts.myAoHalf );
    Vk_AoPassDetail::gBentHalfLayout = ToVkLayout( layouts.myBentHalf );
    Vk_AoPassDetail::gAoBlurLayout   = ToVkLayout( layouts.myAoBlur );

    ReleaseAdoptedTextures( aCore.myGfxRhiDevice, gpu );
    Rhi::CommandListDestroy( cmd );
}

}  // namespace Vk_AoPass

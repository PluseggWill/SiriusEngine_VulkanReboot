// Module: Vk_SsrPass record path — thin facade over Gfx_SsrPass (Rhi).
#include "Vk_SsrPass.h"

#include "../Gfx/Gfx_SsrPass.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <array>
#include <glm/glm.hpp>

namespace Vk_SsrPassDetail {
extern VkImageLayout                  gSsrLayout;
extern std::array< VkImageLayout, 2 > gHistoryLayouts;
}  // namespace Vk_SsrPassDetail

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

}  // namespace

namespace Vk_SsrPass {

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || state.myDescriptorSets[ aFrameIndex ] == VK_NULL_HANDLE || !aCore.myGfxRhiDevice ) {
        return;
    }

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    // Descriptor writes stay in RenderCore (Vulkan descriptor update API).
    UpdateDescriptorSet( aCore, aFrameIndex );

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_SsrPass::GpuResources gpu{};
    gpu.myPipeline = RhiVulkan::PipelineAdopt( state.myComputePipeline );
    gpu.myLayout   = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.mySet      = RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ] );
    gpu.myOutput   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySsrOutput.Image(), state.mySsrOutput.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Rhi_ImageLayout outputLayout = ToRhiLayout( Vk_SsrPassDetail::gSsrLayout );

    Gfx_SsrPass::TraceInput input{};
    input.myWidth                 = extent.width;
    input.myHeight                = extent.height;
    input.myDebugLabels           = aCore.AreCommandDebugLabelsEnabled();
    input.myOutputLayout          = &outputLayout;
    input.myPush.view             = aCore.myPrimaryCamera.myView;
    input.myPush.proj             = aCore.myPrimaryCamera.myProj;
    input.myPush.prevViewProj     = aCore.myTemporalState.myPrevViewProj;
    input.myPush.params           = glm::vec4( aCore.myLightingSettings.mySsrMaxDistance, aCore.myLightingSettings.mySsrMaxRoughness,
                                     aCore.myLightingSettings.mySsrEnabled ? 1.0f : 0.0f, aCore.myLightingSettings.mySsrThickness );
    const uint32_t hiZMaxMip      = Vk_DepthPyramidPass::GetMipLevelCount( aCore ) > 0 ? Vk_DepthPyramidPass::GetMipLevelCount( aCore ) - 1 : 0u;
    input.myPush.trace            = glm::uvec4( aCore.myLightingSettings.mySsrMaxSteps, hiZMaxMip, 0u, 0u );
    input.myPush.screenSize       = glm::vec2( static_cast< float >( extent.width ), static_cast< float >( extent.height ) );
    input.myPush.historyValid     = ( aCore.myTemporalState.myHistoryValid && state.myHistoryReady ) ? 1.0f : 0.0f;
    input.myPush.depthRejectSigma = aCore.myLightingSettings.mySsrHistoryDepthReject;

    Gfx_SsrPass::RecordTrace( cmd, gpu, input );

    Vk_SsrPassDetail::gSsrLayout = ToVkLayout( outputLayout );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myOutput );
    Rhi::CommandListDestroy( cmd );
}

void RecordHistoryUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    Vk_SsrState& state = aCore.mySsrState;
    if ( !state.myInitialized || !Vk_PostProcessPass::HasHybridResolve( aCore ) || !aCore.myGfxRhiDevice ) {
        return;
    }

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    Vk_TextureResource& sceneColor = aCore.myPostProcessState.mySceneColor;
    if ( sceneColor.Image() == VK_NULL_HANDLE ) {
        return;
    }

    const uint32_t      writeIndex = 1u - state.myHistoryWriteIndex;
    Vk_TextureResource& history    = state.myLitHdrHistory[ writeIndex ];

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_SsrPass::GpuResources gpu{};
    gpu.mySceneColor   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, sceneColor.Image(), sceneColor.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    gpu.myHistoryWrite = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, history.Image(), history.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Rhi_ImageLayout sceneLayout   = Rhi_ImageLayout::ShaderReadOnly;
    Rhi_ImageLayout historyLayout = ToRhiLayout( Vk_SsrPassDetail::gHistoryLayouts[ writeIndex ] );

    Gfx_SsrPass::HistoryInput input{};
    input.myWidth         = extent.width;
    input.myHeight        = extent.height;
    input.mySceneLayout   = &sceneLayout;
    input.myHistoryLayout = &historyLayout;

    Gfx_SsrPass::RecordHistoryUpdate( cmd, gpu, input );

    Vk_SsrPassDetail::gHistoryLayouts[ writeIndex ] = ToVkLayout( historyLayout );
    state.myHistoryWriteIndex                       = writeIndex;
    state.myHistoryReady                            = true;

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySceneColor );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myHistoryWrite );
    Rhi::CommandListDestroy( cmd );
}

}  // namespace Vk_SsrPass

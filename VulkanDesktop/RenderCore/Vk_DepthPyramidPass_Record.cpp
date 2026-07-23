// Module: Vk_DepthPyramidPass record path — thin facade over Gfx_DepthPyramidPass::Record (Rhi).
#include "Vk_DepthPyramidPass.h"

#include "../Gfx/Gfx_DepthPyramidPass.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

namespace Vk_DepthPyramidPassDetail {
extern VkImageLayout gPyramidLayout;
}

namespace {

Rhi_ImageLayout ToRhiLayout( VkImageLayout aLayout ) {
    switch ( aLayout ) {
    case VK_IMAGE_LAYOUT_GENERAL:
        return Rhi_ImageLayout::General;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return Rhi_ImageLayout::ShaderReadOnly;
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
    case Rhi_ImageLayout::Undefined:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

Gfx_DepthPyramidPass::GpuResources BuildGpuResources( Vk_Renderer& aCore, Rhi_Device& aRhiDevice, uint32_t aFrameIndex ) {
    Vk_DepthPyramidState&              state = aCore.myDepthPyramidState;
    Gfx_DepthPyramidPass::GpuResources gpu{};

    gpu.myPipeline      = state.myRhiPipeline ? state.myRhiPipeline : RhiVulkan::PipelineAdopt( state.myComputePipeline );
    gpu.myLayout        = state.myRhiLayout ? state.myRhiLayout : RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.myMipLevelCount = state.myMipLevelCount;
    gpu.myPyramid       = RhiVulkan::TextureAdopt( aRhiDevice, state.myPyramid.Image(), state.myPyramid.ImageView(), VK_FORMAT_R32_SFLOAT, state.myMipLevelCount );

    for ( uint32_t mip = 0; mip < state.myMipLevelCount && mip < Gfx_DepthPyramidPass::kMaxMipLevels; ++mip ) {
        gpu.myMipSets[ mip ] =
            state.myRhiSets[ aFrameIndex ][ mip ] ? state.myRhiSets[ aFrameIndex ][ mip ] : RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ][ mip ] );
    }
    return gpu;
}

}  // namespace

namespace Vk_DepthPyramidPass {

void RecordBuild( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    if ( !state.myInitialized || state.myMipLevelCount == 0 || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_DepthPyramidPass::GpuResources gpu = BuildGpuResources( aCore, aCore.myGfxRhiDevice, aFrameIndex );

    Rhi_ImageLayout layout = ToRhiLayout( Vk_DepthPyramidPassDetail::gPyramidLayout );

    Gfx_DepthPyramidPass::RecordInput input{};
    input.myWidth         = width;
    input.myHeight        = height;
    input.myDebugLabels   = aCore.AreCommandDebugLabelsEnabled();
    input.myPyramidLayout = &layout;

    Gfx_DepthPyramidPass::Record( cmd, gpu, input );

    Vk_DepthPyramidPassDetail::gPyramidLayout = ToVkLayout( layout );

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myPyramid );
    Rhi::CommandListDestroy( cmd );
}

}  // namespace Vk_DepthPyramidPass

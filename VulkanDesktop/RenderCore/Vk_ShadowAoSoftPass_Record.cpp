// Module: Vk_ShadowAoSoftPass record path — thin facade over Gfx_ShadowAoSoftPass::Record (Rhi).
#include "Vk_ShadowAoSoftPass.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_ShadowAoSoftPass.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "Vk_AoPass.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowMapPass.h"

namespace Vk_ShadowAoSoftPassDetail {
extern VkImageLayout gSoftPingLayout;
extern VkImageLayout gSoftPongLayout;
}  // namespace Vk_ShadowAoSoftPassDetail

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

}  // namespace

namespace Vk_ShadowAoSoftPass {

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, bool aAoPassRan ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    const Gfx_AoSettings& aoSettings = aCore.myAoSettings;
    if ( !aoSettings.myContactSoftEnabled ) {
        return;
    }

    // Depth must be readable before pack; shared with GBuffer/deferred barrier, stays in RenderCore.
    Vk_ShadowMapPass::CmdBarrierForDeferredRead( aCore, aCommandBuffer );

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    const bool            useAoRaw  = aAoPassRan && aoSettings.myEnabled;
    const VkDescriptorSet packSetVk = useAoRaw ? state.myPackDescriptorSets[ aFrameIndex ] : state.myPackNoAoDescriptorSets[ aFrameIndex ];

    Gfx_ShadowAoSoftPass::GpuResources gpu{};
    gpu.myPackPipeline    = RhiVulkan::PipelineAdopt( state.myPackPipeline );
    gpu.myBlurPipeline    = RhiVulkan::PipelineAdopt( state.myBlurPipeline );
    gpu.myPackLayout      = RhiVulkan::PipelineLayoutAdopt( state.myPackPipelineLayout );
    gpu.myBlurLayout      = RhiVulkan::PipelineLayoutAdopt( state.myBlurPipelineLayout );
    gpu.myPackSet         = RhiVulkan::DescriptorSetAdopt( packSetVk );
    gpu.myBlurHorizSet    = RhiVulkan::DescriptorSetAdopt( state.myBlurHorizDescriptorSets[ aFrameIndex ] );
    gpu.myBlurVertSet     = RhiVulkan::DescriptorSetAdopt( state.myBlurVertDescriptorSets[ aFrameIndex ] );
    gpu.mySoftPing        = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySoftPing.Image(), state.mySoftPing.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
    gpu.mySoftPong        = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySoftPong.Image(), state.mySoftPong.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
    gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Rhi_ImageLayout pingLayout = ToRhiLayout( Vk_ShadowAoSoftPassDetail::gSoftPingLayout );
    Rhi_ImageLayout pongLayout = ToRhiLayout( Vk_ShadowAoSoftPassDetail::gSoftPongLayout );
    Rhi_ImageLayout aoRawLayout{};
    if ( useAoRaw ) {
        gpu.myAoRaw = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myAoState.myAoRaw.Image(), aCore.myAoState.myAoRaw.ImageView(), VK_FORMAT_R8_UNORM, 1 );
        aoRawLayout = ToRhiLayout( Vk_AoPass::GetRawAoLayout() );
    }

    Gfx_ShadowAoSoftPass::RecordInput input{};
    input.myWidth       = width;
    input.myHeight      = height;
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();
    input.myUseAoRaw    = useAoRaw;
    input.myBlurRadius  = aoSettings.myContactSoftBlurRadius;
    input.myDepthSigma  = aoSettings.myContactSoftDepthSigma;
    input.myPingLayout  = &pingLayout;
    input.myPongLayout  = &pongLayout;
    input.myAoRawLayout = useAoRaw ? &aoRawLayout : nullptr;

    Gfx_ShadowAoSoftPass::Record( cmd, gpu, input );

    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = ToVkLayout( pingLayout );
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = ToVkLayout( pongLayout );
    if ( useAoRaw ) {
        Vk_AoPass::NoteRawAoLayout( ToVkLayout( aoRawLayout ) );
        Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myAoRaw );
    }

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySoftPing );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySoftPong );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myGBufferWorldPos );
    Rhi::CommandListDestroy( cmd );
}

}  // namespace Vk_ShadowAoSoftPass

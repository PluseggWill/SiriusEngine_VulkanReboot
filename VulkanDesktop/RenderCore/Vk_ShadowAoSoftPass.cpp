// Module: Vk_ShadowAoSoftPass — SPIR-V load + Init/Record orchestration over Gfx_ShadowAoSoftPass.
#include "Vk_ShadowAoSoftPass.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_ShadowAoSoftPass.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_AoPass.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowMapPass.h"

#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_ShadowAoSoftPass::kMaxFramesInFlight ), "Gfx_ShadowAoSoftPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace Vk_ShadowAoSoftPassDetail {
VkImageLayout gSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}  // namespace Vk_ShadowAoSoftPassDetail

namespace {

constexpr char kPackShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoPack.spv";
constexpr char kBlurShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoBlur.spv";

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

bool CreateOrRefreshImages( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;

    Gfx_ShadowAoSoftPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth          = width;
    imageDesc.myHeight         = height;
    imageDesc.myCreateFallback = true;
    if ( !Gfx_ShadowAoSoftPass::CreateOrRecreateImages( rhi, imageDesc, state.myGfx ) ) {
        return false;
    }

    if ( state.myGfx.myImagesReady ) {
        Vk_ShadowAoSoftPassDetail::gSoftPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Vk_ShadowAoSoftPassDetail::gSoftPongLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        UtilLogger::Info( "SHADOW-AO-SOFT", "contact soft targets: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=RG8_UNORM (Gfx)" );
    }
    return true;
}

void UpdateAllDescriptorSets( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.mySetsAllocated ) {
        return;
    }

    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), aCore.FindDepthFormat(), 1 );
    Rhi_Texture worldPosTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture aoRawTex{};
    if ( aCore.myAoState.myGfx.myAoRaw ) {
        aoRawTex = aCore.myAoState.myGfx.myAoRaw;
    }
    Rhi_Texture shadowDepth = aCore.myShadowMapState.myGfx.myDepth;
    Rhi_Sampler shadowCmp   = aCore.myShadowMapState.myGfx.myCompareSampler;
    Rhi_Buffer  lightingBuf = RhiVulkan::BufferAdopt( aCore.myLightingGlobalsBuffer.myBuffer );

    Gfx_ShadowAoSoftPass::DescriptorUpdateDesc desc{};
    desc.myFramesInFlight        = MAX_FRAMES_IN_FLIGHT;
    desc.myGBufferDepth          = depthTex;
    desc.myGBufferWorldPos       = worldPosTex;
    desc.myAoRaw                 = aoRawTex;
    desc.myShadowDepth           = shadowDepth;
    desc.myShadowCompareSampler  = shadowCmp;
    desc.myLightingGlobals       = lightingBuf;
    desc.myLightingGlobalsStride = aCore.PadUniformBufferSize( sizeof( Gpu_LightingGlobals ) );
    desc.myLightingGlobalsRange  = sizeof( Gpu_LightingGlobals );
    Gfx_ShadowAoSoftPass::UpdateDescriptors( rhi, desc, state.myGfx );

    Rhi::DeviceDestroyTexture( rhi, depthTex );
    Rhi::DeviceDestroyTexture( rhi, worldPosTex );
}

void CreatePipelines( Vk_Renderer& aCore ) {
    const std::string         packSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kPackShaderPath );
    const std::string         blurSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kBlurShaderPath );
    const std::vector< char > packSpirv   = LoadSpirvBytes( packSpvPath );
    const std::vector< char > blurSpirv   = LoadSpirvBytes( blurSpvPath );

    Gfx_ShadowAoSoftPass::PipelineInitDesc pipeDesc{};
    pipeDesc.myPackSpirvCode  = packSpirv.data();
    pipeDesc.myPackSpirvBytes = packSpirv.size();
    pipeDesc.myBlurSpirvCode  = blurSpirv.data();
    pipeDesc.myBlurSpirvBytes = blurSpirv.size();
    pipeDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_ShadowAoSoftPass::CreatePipelines( aCore.myGfxRhiDevice, pipeDesc, aCore.myShadowAoSoftState.myGfx ) ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: Gfx CreatePipelines failed" );
    }
    UtilLogger::Info( "PIPELINE", "Shadow/AO contact soft compute pipelines created (Gfx)." );
}

}  // namespace

namespace Vk_ShadowAoSoftPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_ShadowAoSoftPass::Destroy( aCore.myGfxRhiDevice, aCore.myShadowAoSoftState.myGfx );
    }
    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    aCore.myShadowAoSoftState.myInitialized    = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshImages( aCore ) ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: recreate images failed" );
    }
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: myGfxRhiDevice required for Gfx Init" );
    }
    UtilLogger::Info( "FG", "Vk_ShadowAoSoftPass::Init (Gfx create)." );
    CreatePipelines( aCore );
    if ( !CreateOrRefreshImages( aCore ) ) {
        Gfx_ShadowAoSoftPass::Destroy( aCore.myGfxRhiDevice, aCore.myShadowAoSoftState.myGfx );
        throw std::runtime_error( "Vk_ShadowAoSoftPass: Gfx CreateOrRecreateImages failed" );
    }
    UpdateAllDescriptorSets( aCore );
    aCore.myShadowAoSoftState.myInitialized = true;
}

// RecordCompute via Gfx_ShadowAoSoftPass::Record (Rhi + Vk_FrameCmd).

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

    Vk_ShadowMapPass::CmdBarrierForDeferredRead( aCore, aCommandBuffer );

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    const bool  useAoRaw = aAoPassRan && aoSettings.myEnabled;
    const auto& gfx      = state.myGfx;

    Gfx_ShadowAoSoftPass::GpuResources gpu{};
    gpu.myPackPipeline    = gfx.myPackPipeline;
    gpu.myBlurPipeline    = gfx.myBlurPipeline;
    gpu.myPackLayout      = gfx.myPackLayout;
    gpu.myBlurLayout      = gfx.myBlurLayout;
    gpu.myPackSet         = useAoRaw ? gfx.myPackSets[ aFrameIndex ] : gfx.myPackNoAoSets[ aFrameIndex ];
    gpu.myBlurHorizSet    = gfx.myBlurHorizSets[ aFrameIndex ];
    gpu.myBlurVertSet     = gfx.myBlurVertSets[ aFrameIndex ];
    gpu.mySoftPing        = gfx.mySoftPing;
    gpu.mySoftPong        = gfx.mySoftPong;
    gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Rhi_ImageLayout pingLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_ShadowAoSoftPassDetail::gSoftPingLayout );
    Rhi_ImageLayout pongLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_ShadowAoSoftPassDetail::gSoftPongLayout );
    Rhi_ImageLayout aoRawLayout{};
    if ( useAoRaw ) {
        gpu.myAoRaw = aCore.myAoState.myGfx.myAoRaw;
        aoRawLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_AoPass::GetRawAoLayout() );
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

    Gfx_ShadowAoSoftPass::Record( frameCmd.Get(), gpu, input );

    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = Vk_FrameCmd::ImageLayoutToVk( pingLayout );
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = Vk_FrameCmd::ImageLayoutToVk( pongLayout );
    if ( useAoRaw ) {
        Vk_AoPass::NoteRawAoLayout( Vk_FrameCmd::ImageLayoutToVk( aoRawLayout ) );
    }

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myGBufferWorldPos );
}

}  // namespace Vk_ShadowAoSoftPass

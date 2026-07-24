// Module: Vk_SsrPass — SPIR-V load + Init/Record orchestration over Gfx_SsrPass.
#include "Vk_SsrPass.h"

#include "../Gfx/Gfx_SsrPass.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_DepthPyramidPass.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <glm/glm.hpp>

#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_SsrPass::kMaxFramesInFlight ), "Gfx_SsrPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace Vk_SsrPassDetail {
VkImageLayout                  gSsrLayout = VK_IMAGE_LAYOUT_UNDEFINED;
std::array< VkImageLayout, 2 > gHistoryLayouts{ VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };
}  // namespace Vk_SsrPassDetail

namespace {

constexpr char kSsrShaderPath[] = "VulkanDesktop/Shader_Generated/SsrTrace.spv";

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_SsrPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

bool CreateOrRefreshImages( Vk_Renderer& aCore ) {
    Vk_SsrState& state = aCore.mySsrState;
    Rhi_Device&  rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return false;
    }

    Gfx_SsrPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth  = width;
    imageDesc.myHeight = height;
    if ( !Gfx_SsrPass::CreateOrRecreateImages( rhi, imageDesc, state.myGfx ) ) {
        return false;
    }

    Vk_SsrPassDetail::gHistoryLayouts[ 0 ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Vk_SsrPassDetail::gHistoryLayouts[ 1 ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    UtilLogger::Info( "SSR", "target + lit HDR history: " + std::to_string( width ) + "x" + std::to_string( height ) + " format=RGBA16F (Gfx)" );
    return true;
}

void UpdateDescriptorSetImpl( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;
    Rhi_Device&  rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.mySetsAllocated || !state.myGfx.myImagesReady ) {
        return;
    }

    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), aCore.FindDepthFormat(), 1 );
    Rhi_Texture normalTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture worldPosTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture albedoTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myAlbedo.Image(), aCore.myGBufferState.myAlbedo.ImageView(), VK_FORMAT_R8G8B8A8_UNORM, 1 );

    const uint32_t readIndex   = state.myHistoryWriteIndex;
    Rhi_Texture    historyRead = ( readIndex == 0 ) ? state.myGfx.myHistory0 : state.myGfx.myHistory1;

    Gfx_SsrPass::DescriptorUpdateDesc desc{};
    desc.myFramesInFlight  = MAX_FRAMES_IN_FLIGHT;
    desc.myFrameIndex      = aFrameIndex;
    desc.myGBufferDepth    = depthTex;
    desc.myGBufferNormal   = normalTex;
    desc.myGBufferWorldPos = worldPosTex;
    desc.myGBufferAlbedo   = albedoTex;
    desc.myHistoryRead     = historyRead;
    desc.myHiZ             = aCore.myDepthPyramidState.myGfx.myPyramid;
    desc.myHiZSampler      = aCore.myDepthPyramidState.myGfx.myPyramidSampler;
    Gfx_SsrPass::UpdateDescriptors( rhi, desc, state.myGfx );

    Rhi::DeviceDestroyTexture( rhi, depthTex );
    Rhi::DeviceDestroyTexture( rhi, normalTex );
    Rhi::DeviceDestroyTexture( rhi, worldPosTex );
    Rhi::DeviceDestroyTexture( rhi, albedoTex );
}

void CreatePipeline( Vk_Renderer& aCore ) {
    const std::string         spvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kSsrShaderPath );
    const std::vector< char > spirv   = LoadSpirvBytes( spvPath );

    Gfx_SsrPass::PipelineInitDesc pipeDesc{};
    pipeDesc.mySpirvCode      = spirv.data();
    pipeDesc.mySpirvBytes     = spirv.size();
    pipeDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_SsrPass::CreatePipeline( aCore.myGfxRhiDevice, pipeDesc, aCore.mySsrState.myGfx ) ) {
        throw std::runtime_error( "Vk_SsrPass: Gfx CreatePipeline failed" );
    }
    UtilLogger::Info( "PIPELINE", "SSR compute pipeline created (Gfx)." );
}

}  // namespace

namespace Vk_SsrPass {

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    UpdateDescriptorSetImpl( aCore, aFrameIndex );
}

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_SsrPass::Destroy( aCore.myGfxRhiDevice, aCore.mySsrState.myGfx );
    }
    Vk_SsrPassDetail::gSsrLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    Vk_SsrPassDetail::gHistoryLayouts    = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    aCore.mySsrState.myInitialized       = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshImages( aCore ) ) {
        throw std::runtime_error( "Vk_SsrPass: recreate images failed" );
    }
    Vk_SsrPassDetail::gSsrLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    aCore.mySsrState.myHistoryReady = false;
    UpdateDescriptorSetImpl( aCore, 0xffffffffu );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_SsrPass: myGfxRhiDevice required for Gfx Init" );
    }
    if ( !aCore.myGBufferState.myInitialized || !aCore.myDepthPyramidState.myInitialized || !Vk_PostProcessPass::HasHybridResolve( aCore ) ) {
        throw std::runtime_error( "Vk_SsrPass::Init requires GBuffer, DepthPyramid, and PostProcess hybrid resolve" );
    }
    UtilLogger::Info( "FG", "Vk_SsrPass::Init (Gfx create)." );
    CreatePipeline( aCore );
    if ( !CreateOrRefreshImages( aCore ) ) {
        Gfx_SsrPass::Destroy( aCore.myGfxRhiDevice, aCore.mySsrState.myGfx );
        throw std::runtime_error( "Vk_SsrPass: Gfx CreateOrRecreateImages failed" );
    }
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    UpdateDescriptorSetImpl( aCore, 0xffffffffu );
    aCore.mySsrState.myInitialized = true;
}

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !state.myGfx.mySets[ aFrameIndex ] || !aCore.myGfxRhiDevice ) {
        return;
    }

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    UpdateDescriptorSet( aCore, aFrameIndex );

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    const auto&               gfx = state.myGfx;
    Gfx_SsrPass::GpuResources gpu{};
    gpu.myPipeline = gfx.myPipeline;
    gpu.myLayout   = gfx.myLayout;
    gpu.mySet      = gfx.mySets[ aFrameIndex ];
    gpu.myOutput   = gfx.mySsrOutput;

    Rhi_ImageLayout outputLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_SsrPassDetail::gSsrLayout );

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

    Gfx_SsrPass::RecordTrace( frameCmd.Get(), gpu, input );

    Vk_SsrPassDetail::gSsrLayout = Vk_FrameCmd::ImageLayoutToVk( outputLayout );
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

    const Rhi_Texture sceneColor = aCore.myPostProcessState.myGfx.mySceneColor;
    if ( !sceneColor ) {
        return;
    }

    const uint32_t writeIndex = 1u - state.myHistoryWriteIndex;

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_SsrPass::GpuResources gpu{};
    gpu.mySceneColor   = sceneColor;
    gpu.myHistoryWrite = ( writeIndex == 0u ) ? state.myGfx.myHistory0 : state.myGfx.myHistory1;

    Rhi_ImageLayout sceneLayout   = Rhi_ImageLayout::ShaderReadOnly;
    Rhi_ImageLayout historyLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_SsrPassDetail::gHistoryLayouts[ writeIndex ] );

    Gfx_SsrPass::HistoryInput input{};
    input.myWidth         = extent.width;
    input.myHeight        = extent.height;
    input.mySceneLayout   = &sceneLayout;
    input.myHistoryLayout = &historyLayout;

    Gfx_SsrPass::RecordHistoryUpdate( frameCmd.Get(), gpu, input );

    Vk_SsrPassDetail::gHistoryLayouts[ writeIndex ] = Vk_FrameCmd::ImageLayoutToVk( historyLayout );
    state.myHistoryWriteIndex                       = writeIndex;
    state.myHistoryReady                            = true;
}

}  // namespace Vk_SsrPass

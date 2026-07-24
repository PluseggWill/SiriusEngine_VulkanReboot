// Module: Vk_DepthPyramidPass — SPIR-V load + Init/Record orchestration over Gfx_DepthPyramidPass.
#include "Vk_DepthPyramidPass.h"

#include "../Gfx/Gfx_DepthPyramidPass.h"

static_assert( kHiZMaxMipLevels == Gfx_DepthPyramidPass::kMaxMipLevels, "Hi-Z mip cap must match Gfx_DepthPyramidPass" );

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_DepthPyramidPass::kMaxFramesInFlight ), "Gfx_DepthPyramidPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace {

constexpr char kDepthPyramidShaderPath[] = "VulkanDesktop/Shader_Generated/DepthPyramid.spv";

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

bool CreateOrRefreshImage( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 || !aCore.myGBufferState.myDepth.Image() || !aCore.myGBufferState.myDepth.ImageView() ) {
        return false;
    }

    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), VK_FORMAT_D32_SFLOAT, 1 );

    Gfx_DepthPyramidPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth          = width;
    imageDesc.myHeight         = height;
    imageDesc.myGBufferDepth   = depthTex;
    imageDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    const bool ok              = Gfx_DepthPyramidPass::CreateOrRecreateImage( rhi, imageDesc, state.myGfx );
    Rhi::DeviceDestroyTexture( rhi, depthTex );
    if ( !ok ) {
        return false;
    }

    UtilLogger::Info( "HIZ", "Depth pyramid: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " mips=" + std::to_string( state.myGfx.myMipLevelCount )
                                 + " (mip0=full-res copy)" );
    return true;
}

}  // namespace

namespace Vk_DepthPyramidPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myDepthPyramidState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_DepthPyramidPass::Destroy( aCore.myGfxRhiDevice, aCore.myDepthPyramidState.myGfx );
    }
    aCore.myDepthPyramidState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myDepthPyramidState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshImage( aCore ) ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: recreate image failed" );
    }
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myDepthPyramidState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: myGfxRhiDevice required for Gfx Init" );
    }

    UtilLogger::Info( "FG", "Vk_DepthPyramidPass::Init (Gfx create)." );

    const std::string         spvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDepthPyramidShaderPath );
    const std::vector< char > spirv   = LoadSpirvBytes( spvPath );

    Gfx_DepthPyramidPass::PipelineInitDesc pipeDesc{};
    pipeDesc.mySpirvCode      = spirv.data();
    pipeDesc.mySpirvBytes     = spirv.size();
    pipeDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_DepthPyramidPass::CreatePipeline( aCore.myGfxRhiDevice, pipeDesc, aCore.myDepthPyramidState.myGfx ) ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: Gfx CreatePipeline failed" );
    }
    if ( !CreateOrRefreshImage( aCore ) ) {
        Gfx_DepthPyramidPass::Destroy( aCore.myGfxRhiDevice, aCore.myDepthPyramidState.myGfx );
        throw std::runtime_error( "Vk_DepthPyramidPass: Gfx CreateOrRecreateImage failed" );
    }

    aCore.myDepthPyramidState.myInitialized = true;
    UtilLogger::Info( "PIPELINE", "DepthPyramid compute pipeline created (Gfx)." );
}

void RecordBuild( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    if ( !state.myInitialized || state.myGfx.myMipLevelCount == 0 || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_DepthPyramidPass::GpuResources gpu = Gfx_DepthPyramidPass::MakeGpuResources( state.myGfx, aFrameIndex );

    Gfx_DepthPyramidPass::RecordInput input{};
    input.myWidth         = width;
    input.myHeight        = height;
    input.myDebugLabels   = aCore.AreCommandDebugLabelsEnabled();
    input.myPyramidLayout = &state.myGfx.myPyramidLayout;

    Gfx_DepthPyramidPass::Record( frameCmd.Get(), gpu, input );
}

uint32_t GetMipLevelCount( const Vk_Renderer& aCore ) {
    return aCore.myDepthPyramidState.myGfx.myMipLevelCount;
}

}  // namespace Vk_DepthPyramidPass

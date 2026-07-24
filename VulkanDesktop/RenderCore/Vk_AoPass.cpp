// Module: Vk_AoPass — SPIR-V load + Init/Record orchestration over Gfx_AoPass.
#include "Vk_AoPass.h"

#include "Vk_AoPassInternal.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoPass.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_AoPass::kMaxFramesInFlight ), "Gfx_AoPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace {

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_AoPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

constexpr char kClassicShaderPath[]  = "VulkanDesktop/Shader_Generated/Ssao.spv";
constexpr char kHbaoShaderPath[]     = "VulkanDesktop/Shader_Generated/HbaoPlus.spv";
constexpr char kGtaoShaderPath[]     = "VulkanDesktop/Shader_Generated/Gtao.spv";
constexpr char kUpsampleShaderPath[] = "VulkanDesktop/Shader_Generated/AoUpsample.spv";
constexpr char kBlurShaderPath[]     = "VulkanDesktop/Shader_Generated/SsaoBlur.spv";
constexpr char kTemporalShaderPath[] = "VulkanDesktop/Shader_Generated/AoTemporal.spv";

VkExtent2D HalfExtent( uint32_t aWidth, uint32_t aHeight ) {
    return { std::max( 1u, ( aWidth + 1u ) / 2u ), std::max( 1u, ( aHeight + 1u ) / 2u ) };
}

bool CreateOrRefreshImages( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    Rhi_Device& rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return false;
    }

    Gfx_AoPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth          = width;
    imageDesc.myHeight         = height;
    imageDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_AoPass::CreateOrRecreateImages( rhi, imageDesc, state.myGfx ) ) {
        return false;
    }

    Vk_AoPassDetail::gBentHalfLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    UtilLogger::Info( "AO", "targets: full=" + std::to_string( width ) + "x" + std::to_string( height ) + " half=" + std::to_string( HalfExtent( width, height ).width ) + "x"
                                + std::to_string( HalfExtent( width, height ).height ) + " format=R8_UNORM (Gfx)" );
    return true;
}

void CreatePipelines( Vk_Renderer& aCore ) {
    const std::string         classicSpv    = UtilLoader::ResolvePath( aCore.EngineConfig(), kClassicShaderPath );
    const std::string         hbaoSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kHbaoShaderPath );
    const std::string         gtaoSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kGtaoShaderPath );
    const std::string         upsampleSpv   = UtilLoader::ResolvePath( aCore.EngineConfig(), kUpsampleShaderPath );
    const std::string         blurSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kBlurShaderPath );
    const std::string         temporalSpv   = UtilLoader::ResolvePath( aCore.EngineConfig(), kTemporalShaderPath );
    const std::vector< char > classicBytes  = LoadSpirvBytes( classicSpv );
    const std::vector< char > hbaoBytes     = LoadSpirvBytes( hbaoSpv );
    const std::vector< char > gtaoBytes     = LoadSpirvBytes( gtaoSpv );
    const std::vector< char > upsampleBytes = LoadSpirvBytes( upsampleSpv );
    const std::vector< char > blurBytes     = LoadSpirvBytes( blurSpv );
    const std::vector< char > temporalBytes = LoadSpirvBytes( temporalSpv );

    Gfx_AoPass::PipelineInitDesc pipeDesc{};
    pipeDesc.myClassicSpirvCode   = classicBytes.data();
    pipeDesc.myClassicSpirvBytes  = classicBytes.size();
    pipeDesc.myHbaoSpirvCode      = hbaoBytes.data();
    pipeDesc.myHbaoSpirvBytes     = hbaoBytes.size();
    pipeDesc.myGtaoSpirvCode      = gtaoBytes.data();
    pipeDesc.myGtaoSpirvBytes     = gtaoBytes.size();
    pipeDesc.myUpsampleSpirvCode  = upsampleBytes.data();
    pipeDesc.myUpsampleSpirvBytes = upsampleBytes.size();
    pipeDesc.myBlurSpirvCode      = blurBytes.data();
    pipeDesc.myBlurSpirvBytes     = blurBytes.size();
    pipeDesc.myTemporalSpirvCode  = temporalBytes.data();
    pipeDesc.myTemporalSpirvBytes = temporalBytes.size();
    pipeDesc.myFramesInFlight     = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_AoPass::CreatePipelines( aCore.myGfxRhiDevice, pipeDesc, aCore.myAoState.myGfx ) ) {
        throw std::runtime_error( "Vk_AoPass: Gfx CreatePipelines failed" );
    }
    UtilLogger::Info( "PIPELINE", "AO compute pipelines created (Classic SSAO, HBAO+, GTAO, upsample, blur) — Gfx." );
}

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

void UpdateAllDescriptorSets( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    Rhi_Device& rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.mySetsAllocated || !state.myGfx.myImagesReady ) {
        return;
    }

    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), aCore.FindDepthFormat(), 1 );
    Rhi_Texture normalTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture worldPosTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    Rhi_Texture motionTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myMotionVector.Image(), aCore.myGBufferState.myMotionVector.ImageView(), VK_FORMAT_R16G16_SFLOAT, 1 );

    Gfx_AoPass::DescriptorUpdateDesc desc{};
    desc.myFramesInFlight    = MAX_FRAMES_IN_FLIGHT;
    desc.myGBufferDepth      = depthTex;
    desc.myGBufferNormal     = normalTex;
    desc.myGBufferWorldPos   = worldPosTex;
    desc.myMotionVector      = motionTex;
    desc.myTemporalReadIndex = state.myTemporalReadIndex;
    Gfx_AoPass::UpdateDescriptors( rhi, desc, state.myGfx );

    Rhi::DeviceDestroyTexture( rhi, depthTex );
    Rhi::DeviceDestroyTexture( rhi, normalTex );
    Rhi::DeviceDestroyTexture( rhi, worldPosTex );
    Rhi::DeviceDestroyTexture( rhi, motionTex );
}

void UpdateTemporalDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;
    Rhi_Device& rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.mySetsAllocated ) {
        return;
    }

    Rhi_Texture motionTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myMotionVector.Image(), aCore.myGBufferState.myMotionVector.ImageView(), VK_FORMAT_R16G16_SFLOAT, 1 );

    Gfx_AoPass::DescriptorUpdateDesc desc{};
    desc.myMotionVector      = motionTex;
    desc.myTemporalReadIndex = state.myTemporalReadIndex;
    Gfx_AoPass::UpdateTemporalDescriptors( rhi, desc, state.myGfx, aFrameIndex );

    Rhi::DeviceDestroyTexture( rhi, motionTex );
}

}  // namespace

namespace Vk_AoPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myAoState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_AoPass::Destroy( aCore.myGfxRhiDevice, aCore.myAoState.myGfx );
    }
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    aCore.myAoState.myInitialized          = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myAoState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshImages( aCore ) ) {
        throw std::runtime_error( "Vk_AoPass: recreate images failed" );
    }
    Vk_AoPassDetail::ResetImageLayouts();
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myAoState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_AoPass: myGfxRhiDevice required for Gfx Init" );
    }
    UtilLogger::Info( "FG", "Vk_AoPass::Init (Gfx create)." );
    CreatePipelines( aCore );
    if ( !CreateOrRefreshImages( aCore ) ) {
        Gfx_AoPass::Destroy( aCore.myGfxRhiDevice, aCore.myAoState.myGfx );
        throw std::runtime_error( "Vk_AoPass: Gfx CreateOrRecreateImages failed" );
    }
    UpdateAllDescriptorSets( aCore );
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    aCore.myAoState.myInitialized          = true;
}

void NoteRawAoLayout( VkImageLayout aLayout ) {
    Vk_AoPassDetail::gAoRawLayout = aLayout;
}

VkImageLayout GetRawAoLayout() {
    return Vk_AoPassDetail::gAoRawLayout;
}

namespace {

    Rhi_ImageLayout AoToRhiLayout( VkImageLayout aLayout ) {
        return Vk_FrameCmd::ImageLayoutFromVk( aLayout );
    }

    VkImageLayout AoToVkLayout( Rhi_ImageLayout aLayout ) {
        return Vk_FrameCmd::ImageLayoutToVk( aLayout );
    }

    void PrepareTemporalDescriptorsThunk( void* aUser, uint32_t aFrameIndex ) {
        UpdateTemporalDescriptorSet( *static_cast< Vk_Renderer* >( aUser ), aFrameIndex );
    }

    Gfx_AoPass::GpuResources BuildAoGpuResources( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
        const auto&              gfx = aCore.myAoState.myGfx;
        Gfx_AoPass::GpuResources gpu{};

        gpu.myClassicPipeline  = gfx.myClassicPipeline;
        gpu.myHbaoPipeline     = gfx.myHbaoPipeline;
        gpu.myGtaoPipeline     = gfx.myGtaoPipeline;
        gpu.myUpsamplePipeline = gfx.myUpsamplePipeline;
        gpu.myBlurPipeline     = gfx.myBlurPipeline;
        gpu.myTemporalPipeline = gfx.myTemporalPipeline;

        gpu.myClassicLayout  = gfx.myClassicLayout;
        gpu.myHalfResLayout  = gfx.myHalfResLayout;
        gpu.myUpsampleLayout = gfx.myUpsampleLayout;
        gpu.myBlurLayout     = gfx.myBlurLayout;
        gpu.myTemporalLayout = gfx.myTemporalLayout;

        gpu.myClassicSet   = gfx.myClassicSets[ aFrameIndex ];
        gpu.myHalfResSet   = gfx.myHalfResSets[ aFrameIndex ];
        gpu.myUpsampleSet  = gfx.myUpsampleSets[ aFrameIndex ];
        gpu.myBlurHorizSet = gfx.myBlurHorizSets[ aFrameIndex ];
        gpu.myBlurVertSet  = gfx.myBlurVertSets[ aFrameIndex ];
        gpu.myTemporalSet  = gfx.myTemporalSets[ aFrameIndex ];

        gpu.myAoRaw           = gfx.myAoRaw;
        gpu.myAoHalf          = gfx.myAoHalf;
        gpu.myBentNormalHalf  = gfx.myBentNormalHalf;
        gpu.myAoBlur          = gfx.myAoBlur;
        gpu.myAoHistory0      = gfx.myAoHistory0;
        gpu.myAoHistory1      = gfx.myAoHistory1;
        gpu.myGBufferNormal   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myGBufferState.myNormalRoughness.Image(),
                                                         aCore.myGBufferState.myNormalRoughness.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
        gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                         VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
        return gpu;
    }

    void ReleaseAdoptedAoTextures( Rhi_Device& aRhiDevice, Gfx_AoPass::GpuResources& aGpu ) {
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferNormal );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferWorldPos );
    }

}  // namespace

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

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_AoPass::GpuResources gpu = BuildAoGpuResources( aCore, aFrameIndex );

    Gfx_AoPass::LayoutState layouts{};
    layouts.myAoRaw    = AoToRhiLayout( Vk_AoPassDetail::gAoRawLayout );
    layouts.myAoHalf   = AoToRhiLayout( Vk_AoPassDetail::gAoHalfLayout );
    layouts.myBentHalf = AoToRhiLayout( Vk_AoPassDetail::gBentHalfLayout );
    layouts.myAoBlur   = AoToRhiLayout( Vk_AoPassDetail::gAoBlurLayout );

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

    Gfx_AoPass::Record( frameCmd.Get(), gpu, input );

    Vk_AoPassDetail::gAoRawLayout    = AoToVkLayout( layouts.myAoRaw );
    Vk_AoPassDetail::gAoHalfLayout   = AoToVkLayout( layouts.myAoHalf );
    Vk_AoPassDetail::gBentHalfLayout = AoToVkLayout( layouts.myBentHalf );
    Vk_AoPassDetail::gAoBlurLayout   = AoToVkLayout( layouts.myAoBlur );

    ReleaseAdoptedAoTextures( aCore.myGfxRhiDevice, gpu );
}

}  // namespace Vk_AoPass

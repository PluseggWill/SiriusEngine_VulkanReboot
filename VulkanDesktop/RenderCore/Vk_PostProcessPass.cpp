// Module: Vk_PostProcessPass — HDR hybrid resolve target + tonemap/bloom to swapchain.
#include "Vk_PostProcessPass.h"

#include "Vk_PostProcessPassInternal.h"

#include "../Gfx/Gfx_PostProcessPass.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameLimits.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include "../Rhi/Rhi_Device.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char kTonemapVertSpv[]    = "VulkanDesktop/Shader_Generated/TonemapVert.spv";
constexpr char kTonemapFragSpv[]    = "VulkanDesktop/Shader_Generated/TonemapFrag.spv";
constexpr char kBloomThresholdSpv[] = "VulkanDesktop/Shader_Generated/BloomThreshold.spv";
constexpr char kBloomBlurSpv[]      = "VulkanDesktop/Shader_Generated/BloomBlur.spv";
constexpr char kTaaResolveSpv[]     = "VulkanDesktop/Shader_Generated/TaaResolve.spv";

VkExtent2D BloomExtent( const VkExtent2D& aFullExtent ) {
    return { std::max( 1u, aFullExtent.width / 2 ), std::max( 1u, aFullExtent.height / 2 ) };
}

bool CreateOrRefreshImages( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        return false;
    }
    Vk_PostProcessState& state = aCore.myPostProcessState;

    Gfx_PostProcessPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    imageDesc.myHeight = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( !Gfx_PostProcessPass::CreateOrRecreateImages( aCore.myGfxRhiDevice, imageDesc, state.myGfx ) ) {
        return false;
    }
    return state.myGfx.myImagesReady;
}

void UpdateComputeDescriptors( Vk_Renderer& aCore, uint32_t aFrameIndex = 0xffffffffu ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    Rhi_Device&          rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.mySetsAllocated || !state.myGfx.myImagesReady ) {
        return;
    }

    Rhi_Texture motion =
        RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myMotionVector.Image(), aCore.myGBufferState.myMotionVector.ImageView(), VK_FORMAT_R16G16_SFLOAT, 1 );

    Gfx_PostProcessPass::DescriptorUpdateDesc desc{};
    desc.myFramesInFlight      = MAX_FRAMES_IN_FLIGHT;
    desc.myTaaHistoryReadIndex = 1u - state.myGfx.myTaaHistoryWriteIndex;
    desc.myFrameIndex          = aFrameIndex;
    desc.myMotionVector        = motion;
    Gfx_PostProcessPass::UpdateComputeDescriptors( rhi, desc, state.myGfx );
    Rhi::DeviceDestroyTexture( rhi, motion );
}

void UpdateTonemapDescriptors( Vk_Renderer& aCore, uint32_t aFrameIndex = 0xffffffffu ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    Rhi_Device&          rhi   = aCore.myGfxRhiDevice;
    if ( !rhi || !state.myGfx.myTonemapReady || !state.myGfx.myImagesReady ) {
        return;
    }

    Gfx_PostProcessPass::TonemapDescriptorUpdateDesc desc{};
    desc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    desc.myFrameIndex     = aFrameIndex;
    desc.myUseTaaResolved = aCore.myPostSettings.myTaaEnabled && static_cast< bool >( state.myGfx.myTaaResolved );
    Gfx_PostProcessPass::UpdateTonemapDescriptors( rhi, desc, state.myGfx );
}

uint32_t SwapchainSampleCount( const Vk_Renderer& aCore ) {
    switch ( aCore.mySwapchainCtx.myMSAASamples ) {
    case VK_SAMPLE_COUNT_2_BIT:
        return 2;
    case VK_SAMPLE_COUNT_4_BIT:
        return 4;
    case VK_SAMPLE_COUNT_8_BIT:
        return 8;
    default:
        return 1;
    }
}

void CreateHybridRenderPass( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_PostProcessPass: myGfxRhiDevice required for hybrid render pass" );
    }

    Rhi_Device& rhi          = aCore.myGfxRhiDevice;
    uint32_t    depthSamples = 1;
    switch ( aCore.mySwapchainCtx.myMSAASamples ) {
    case VK_SAMPLE_COUNT_2_BIT:
        depthSamples = 2;
        break;
    case VK_SAMPLE_COUNT_4_BIT:
        depthSamples = 4;
        break;
    case VK_SAMPLE_COUNT_8_BIT:
        depthSamples = 8;
        break;
    default:
        depthSamples = 1;
        break;
    }

    Rhi_Format depthFormat = Rhi_Format::Undefined;
    switch ( aCore.FindDepthFormat() ) {
    case VK_FORMAT_D32_SFLOAT:
        depthFormat = Rhi_Format::D32_Sfloat;
        break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        depthFormat = Rhi_Format::D32_Sfloat_S8_Uint;
        break;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        depthFormat = Rhi_Format::D24_Unorm_S8_Uint;
        break;
    default:
        throw std::runtime_error( "Vk_PostProcessPass: unsupported depth format for hybrid render pass" );
    }

    std::array< Rhi::AttachmentDesc, 2 > attachments{};
    attachments[ 0 ].myFormat        = Rhi_Format::RGBA16_Sfloat;
    attachments[ 0 ].myLoadOp        = Rhi_AttachmentLoadOp::Clear;
    attachments[ 0 ].myStoreOp       = Rhi_AttachmentStoreOp::Store;
    attachments[ 0 ].myInitialLayout = Rhi_ImageLayout::Undefined;
    attachments[ 0 ].myFinalLayout   = Rhi_ImageLayout::ShaderReadOnly;
    attachments[ 0 ].mySampleCount   = 1;

    attachments[ 1 ].myFormat        = depthFormat;
    attachments[ 1 ].myLoadOp        = Rhi_AttachmentLoadOp::Load;
    attachments[ 1 ].myStoreOp       = Rhi_AttachmentStoreOp::Store;
    attachments[ 1 ].myInitialLayout = Rhi_ImageLayout::DepthStencilAttachment;
    attachments[ 1 ].myFinalLayout   = Rhi_ImageLayout::DepthStencilAttachment;
    attachments[ 1 ].mySampleCount   = depthSamples;

    const uint32_t      colorIdx = 0;
    Rhi::RenderPassDesc rpDesc{};
    rpDesc.myAttachments                 = attachments.data();
    rpDesc.myAttachmentCount             = static_cast< uint32_t >( attachments.size() );
    rpDesc.myColorAttachmentIndices      = &colorIdx;
    rpDesc.myColorAttachmentCount        = 1;
    rpDesc.myHasDepthStencil             = true;
    rpDesc.myDepthStencilAttachmentIndex = 1;

    aCore.myPostProcessState.myRhiHybridRenderPass = Rhi::DeviceCreateRenderPass( rhi, rpDesc );
    aCore.myPostProcessState.myHybridRenderPass    = RhiVulkan::RenderPassGetVk( rhi, aCore.myPostProcessState.myRhiHybridRenderPass );
    if ( !aCore.myPostProcessState.myRhiHybridRenderPass || aCore.myPostProcessState.myHybridRenderPass == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_PostProcessPass: DeviceCreateRenderPass hybrid failed" );
    }
}

void CreateHybridFramebuffer( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice || !aCore.myPostProcessState.myRhiHybridRenderPass || !aCore.myPostProcessState.myGfx.mySceneColor ) {
        throw std::runtime_error( "Vk_PostProcessPass: hybrid render pass + Gfx scene color required before framebuffer" );
    }

    Rhi_Device& rhi      = aCore.myGfxRhiDevice;
    Rhi_Texture colorTex = aCore.myPostProcessState.myGfx.mySceneColor;
    Rhi_Texture depthTex =
        RhiVulkan::TextureAdopt( rhi, aCore.mySwapchainCtx.myDepthTexture.Image(), aCore.mySwapchainCtx.myDepthTexture.ImageView(), aCore.FindDepthFormat(), 1 );
    const std::array< Rhi_Texture, 2 > attachments = { colorTex, depthTex };

    Rhi::FramebufferDesc fbDesc{};
    fbDesc.myRenderPass      = aCore.myPostProcessState.myRhiHybridRenderPass;
    fbDesc.myAttachments     = attachments.data();
    fbDesc.myAttachmentCount = static_cast< uint32_t >( attachments.size() );
    fbDesc.myWidth           = aCore.mySwapchainCtx.mySwapChainExtent.width;
    fbDesc.myHeight          = aCore.mySwapchainCtx.mySwapChainExtent.height;

    aCore.myPostProcessState.myRhiHybridFramebuffer = Rhi::DeviceCreateFramebuffer( rhi, fbDesc );
    aCore.myPostProcessState.myHybridFramebuffer    = RhiVulkan::FramebufferGetVk( rhi, aCore.myPostProcessState.myRhiHybridFramebuffer );
    Rhi::DeviceDestroyTexture( rhi, depthTex );
    if ( !aCore.myPostProcessState.myRhiHybridFramebuffer || aCore.myPostProcessState.myHybridFramebuffer == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_PostProcessPass: DeviceCreateFramebuffer hybrid failed" );
    }
}

std::vector< char > LoadSpirvFile( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_PostProcessPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

void DestroyHybridResolveRhi( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        aCore.myPostProcessState.myHybridFramebuffer    = VK_NULL_HANDLE;
        aCore.myPostProcessState.myHybridRenderPass     = VK_NULL_HANDLE;
        aCore.myPostProcessState.myRhiHybridFramebuffer = {};
        aCore.myPostProcessState.myRhiHybridRenderPass  = {};
        return;
    }
    Rhi_Device& rhi = aCore.myGfxRhiDevice;
    Rhi::DeviceDestroyFramebuffer( rhi, aCore.myPostProcessState.myRhiHybridFramebuffer );
    Rhi::DeviceDestroyRenderPass( rhi, aCore.myPostProcessState.myRhiHybridRenderPass );
    aCore.myPostProcessState.myHybridFramebuffer = VK_NULL_HANDLE;
    aCore.myPostProcessState.myHybridRenderPass  = VK_NULL_HANDLE;
}

bool CreateGfxComputeResources( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        return false;
    }
    Vk_PostProcessState& state = aCore.myPostProcessState;
    if ( state.myGfx.myComputeReady ) {
        return true;
    }

    const std::vector< char > thresholdSpv = LoadSpirvFile( UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomThresholdSpv ) );
    const std::vector< char > blurSpv      = LoadSpirvFile( UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomBlurSpv ) );
    const std::vector< char > taaSpv       = LoadSpirvFile( UtilLoader::ResolvePath( aCore.EngineConfig(), kTaaResolveSpv ) );

    Gfx_PostProcessPass::ComputeInitDesc desc{};
    desc.myThresholdSpirv      = thresholdSpv.data();
    desc.myThresholdSpirvBytes = thresholdSpv.size();
    desc.myBlurSpirv           = blurSpv.data();
    desc.myBlurSpirvBytes      = blurSpv.size();
    desc.myTaaSpirv            = taaSpv.data();
    desc.myTaaSpirvBytes       = taaSpv.size();
    desc.myFramesInFlight      = MAX_FRAMES_IN_FLIGHT;
    return Gfx_PostProcessPass::CreateComputeResources( aCore.myGfxRhiDevice, desc, state.myGfx );
}

bool CreateGfxTonemapResources( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice || aCore.mySwapchainCtx.myRenderPass == VK_NULL_HANDLE ) {
        return false;
    }
    Vk_PostProcessState& state = aCore.myPostProcessState;

    const std::vector< char > vertSpv = LoadSpirvFile( UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapVertSpv ) );
    const std::vector< char > fragSpv = LoadSpirvFile( UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapFragSpv ) );

    Gfx_PostProcessPass::TonemapInitDesc desc{};
    desc.myVertSpirv      = vertSpv.data();
    desc.myVertSpirvBytes = vertSpv.size();
    desc.myFragSpirv      = fragSpv.data();
    desc.myFragSpirvBytes = fragSpv.size();
    desc.myRenderPass     = RhiVulkan::RenderPassAdopt( aCore.mySwapchainCtx.myRenderPass );
    desc.mySampleCount    = SwapchainSampleCount( aCore );
    desc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;

    return state.myGfx.myTonemapReady ? Gfx_PostProcessPass::CreateOrRecreateTonemapPipeline( aCore.myGfxRhiDevice, desc, state.myGfx )
                                      : Gfx_PostProcessPass::CreateTonemapResources( aCore.myGfxRhiDevice, desc, state.myGfx );
}

void CreatePipelineResources( Vk_Renderer& aCore ) {
    if ( aCore.mySwapchainCtx.myRenderPass == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_PostProcessPass: swapchain render pass is required" );
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_PostProcessPass: myGfxRhiDevice required for Gfx Init" );
    }

    if ( !CreateGfxComputeResources( aCore ) ) {
        throw std::runtime_error( "Vk_PostProcessPass: Gfx CreateComputeResources failed" );
    }
    if ( !CreateGfxTonemapResources( aCore ) ) {
        throw std::runtime_error( "Vk_PostProcessPass: Gfx CreateTonemapResources failed" );
    }
}

void RebuildResources( Vk_Renderer& aCore ) {
    if ( aCore.myRhi.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_PostProcessPass: myGfxRhiDevice required for RebuildResources" );
    }

    // Extent-scoped only (RP/FB). Sampler + compute/tonemap layouts live for pass lifetime — never destroy them here.
    DestroyHybridResolveRhi( aCore );
    aCore.myPostProcessState.myDeletionQueue.flush();
    Vk_PostProcessPassDetail::ResetImageLayouts();

    Vk_PostProcessState& state         = aCore.myPostProcessState;
    state.myGfx.myTaaHistoryReady      = false;
    state.myGfx.myTaaHistoryWriteIndex = 0u;

    if ( !CreateOrRefreshImages( aCore ) ) {
        throw std::runtime_error( "Vk_PostProcessPass: Gfx CreateOrRecreateImages failed" );
    }

    CreateHybridRenderPass( aCore );
    CreateHybridFramebuffer( aCore );

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    UtilLogger::Info( "POST", "HDR scene color: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=R16G16B16A16_SFLOAT (Gfx)" );

    if ( state.myGfx.mySetsAllocated ) {
        UpdateComputeDescriptors( aCore );
    }
    if ( state.myGfx.myTonemapReady ) {
        UpdateTonemapDescriptors( aCore );
    }
    if ( !CreateGfxTonemapResources( aCore ) ) {
        throw std::runtime_error( "Vk_PostProcessPass: Gfx CreateOrRecreateTonemapPipeline failed" );
    }
}

void RecordBloom( Vk_Renderer& aCore, Rhi_CommandList& aCmd, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const auto&             gfx   = state.myGfx;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myBloomEnabled ) {
        return;
    }

    const VkExtent2D bloomExtent = BloomExtent( aCore.mySwapchainCtx.mySwapChainExtent );

    Gfx_PostProcessPass::BloomGpu gpu{};
    gpu.myThresholdPipeline = gfx.myThresholdPipeline;
    gpu.myThresholdLayout   = gfx.myThresholdLayout;
    gpu.myThresholdSet      = gfx.myThresholdSets[ aFrameIndex ];
    gpu.myBlurPipeline      = gfx.myBlurPipeline;
    gpu.myBlurLayout        = gfx.myBlurLayout;
    gpu.myBlurHorizSet      = gfx.myBlurHorizSets[ aFrameIndex ];
    gpu.myBlurVertSet       = gfx.myBlurVertSets[ aFrameIndex ];
    gpu.mySceneColor        = gfx.mySceneColor;
    gpu.myBloomPing         = gfx.myBloomPing;
    gpu.myBloomPong         = gfx.myBloomPong;

    Rhi_ImageLayout sceneLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout pingLayout  = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gBloomPingLayout );
    Rhi_ImageLayout pongLayout  = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gBloomPongLayout );

    Gfx_PostProcessPass::BloomInput input{};
    input.myWidth       = bloomExtent.width;
    input.myHeight      = bloomExtent.height;
    input.myThreshold   = post.myBloomThreshold;
    input.mySceneLayout = &sceneLayout;
    input.myPingLayout  = &pingLayout;
    input.myPongLayout  = &pongLayout;

    Gfx_PostProcessPass::RecordBloom( aCmd, gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout = Vk_FrameCmd::ImageLayoutToVk( sceneLayout );
    Vk_PostProcessPassDetail::gBloomPingLayout  = Vk_FrameCmd::ImageLayoutToVk( pingLayout );
    Vk_PostProcessPassDetail::gBloomPongLayout  = Vk_FrameCmd::ImageLayoutToVk( pongLayout );
}

void RecordTaaResolve( Vk_Renderer& aCore, Rhi_CommandList& aCmd, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    auto&                   gfx   = state.myGfx;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myTaaEnabled ) {
        gfx.myTaaHistoryReady = false;
        return;
    }

    if ( !aCore.myTemporalState.myHistoryValid ) {
        gfx.myTaaHistoryReady = false;
    }

    const uint32_t writeIndex = gfx.myTaaHistoryWriteIndex;
    const uint32_t readIndex  = 1u - writeIndex;
    UpdateComputeDescriptors( aCore, aFrameIndex );

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;

    Gfx_PostProcessPass::TaaGpu gpu{};
    gpu.myPipeline     = gfx.myTaaPipeline;
    gpu.myLayout       = gfx.myTaaLayout;
    gpu.mySet          = gfx.myTaaSets[ aFrameIndex ];
    gpu.mySceneColor   = gfx.mySceneColor;
    gpu.myResolved     = gfx.myTaaResolved;
    gpu.myHistoryRead  = ( readIndex == 0u ) ? gfx.myTaaHistory0 : gfx.myTaaHistory1;
    gpu.myHistoryWrite = ( writeIndex == 0u ) ? gfx.myTaaHistory0 : gfx.myTaaHistory1;

    Rhi_ImageLayout sceneLayout     = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout resolvedLayout  = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gTaaResolvedLayout );
    Rhi_ImageLayout histReadLayout  = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] );
    Rhi_ImageLayout histWriteLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] );

    Gfx_PostProcessPass::TaaInput input{};
    input.myWidth              = extent.width;
    input.myHeight             = extent.height;
    input.myPush.historyBlend  = post.myTaaBlend;
    input.myPush.historyValid  = ( aCore.myTemporalState.myHistoryValid && gfx.myTaaHistoryReady ) ? 1.0f : 0.0f;
    input.myPush.varianceGamma = post.myTaaVarianceGamma;
    input.myPush.sharpen       = post.myTaaSharpen;
    input.mySceneLayout        = &sceneLayout;
    input.myResolvedLayout     = &resolvedLayout;
    input.myHistoryReadLayout  = &histReadLayout;
    input.myHistoryWriteLayout = &histWriteLayout;

    Gfx_PostProcessPass::RecordTaa( aCmd, gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout                = Vk_FrameCmd::ImageLayoutToVk( sceneLayout );
    Vk_PostProcessPassDetail::gTaaResolvedLayout               = Vk_FrameCmd::ImageLayoutToVk( resolvedLayout );
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ]  = Vk_FrameCmd::ImageLayoutToVk( histReadLayout );
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = Vk_FrameCmd::ImageLayoutToVk( histWriteLayout );
    gfx.myTaaHistoryWriteIndex                                 = 1u - gfx.myTaaHistoryWriteIndex;
    gfx.myTaaHistoryReady                                      = true;
}

}  // namespace

namespace Vk_PostProcessPass {

bool HasHybridResolve( const Vk_Renderer& aCore ) {
    return aCore.myPostProcessState.myInitialized && aCore.myPostProcessState.myHybridRenderPass != VK_NULL_HANDLE;
}

void MarkSceneColorShaderRead() {
    Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myPostProcessState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }

    Vk_PostProcessState& state = aCore.myPostProcessState;
    state.myDeletionQueue.flush();
    DestroyHybridResolveRhi( aCore );
    if ( aCore.myGfxRhiDevice ) {
        Gfx_PostProcessPass::Destroy( aCore.myGfxRhiDevice, state.myGfx );
    }
    state.myInitialized = false;
    Vk_PostProcessPassDetail::ResetImageLayouts();
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myPostProcessState.myInitialized ) {
        return;
    }
    RebuildResources( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myPostProcessState.myInitialized ) {
        RebuildResources( aCore );
        return;
    }
    if ( !aCore.RenderFeatures().myHybridDeferred ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_PostProcessPass::Init." );
    CreatePipelineResources( aCore );
    RebuildResources( aCore );
    aCore.myPostProcessState.myInitialized = true;
}

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    RecordTaaResolve( aCore, frameCmd.Get(), aFrameIndex );
    RecordBloom( aCore, frameCmd.Get(), aFrameIndex );

    const Gfx_PostSettings& post   = aCore.myPostSettings;
    const VkExtent2D        extent = aCore.mySwapchainCtx.mySwapChainExtent;

    UpdateTonemapDescriptors( aCore, aFrameIndex );

    Gfx_PostProcessPass::TonemapGpu gpu{};
    gpu.myPipeline    = state.myGfx.myTonemapPipeline;
    gpu.myLayout      = state.myGfx.myTonemapLayout;
    gpu.mySet         = state.myGfx.myTonemapSets[ aFrameIndex ];
    gpu.myRenderPass  = RhiVulkan::RenderPassAdopt( aCore.mySwapchainCtx.myRenderPass );
    gpu.myFramebuffer = RhiVulkan::FramebufferAdopt( aCore.mySwapchainCtx.mySwapChainFrameBuffers[ aImageIndex ] );
    gpu.mySceneColor  = state.myGfx.mySceneColor;
    gpu.myBloomPing   = state.myGfx.myBloomPing;

    Rhi_ImageLayout sceneLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gSceneColorLayout );
    Rhi_ImageLayout pingLayout  = Vk_FrameCmd::ImageLayoutFromVk( Vk_PostProcessPassDetail::gBloomPingLayout );

    Gfx_PostProcessPass::TonemapInput input{};
    input.myWidth               = extent.width;
    input.myHeight              = extent.height;
    input.myDebugLabels         = aCore.AreCommandDebugLabelsEnabled();
    input.myBloomEnabled        = post.myBloomEnabled;
    input.mySceneLayout         = &sceneLayout;
    input.myPingLayout          = &pingLayout;
    input.myPush.exposure       = post.myExposure;
    input.myPush.bloomIntensity = post.myBloomIntensity;
    input.myPush.tonemapEnabled = post.myTonemapEnabled ? 1u : 0u;
    input.myPush.bloomEnabled   = post.myBloomEnabled ? 1u : 0u;
    input.myPush.tonemapMode    = post.myTonemapMode != 0 ? 1u : 0u;

    Gfx_PostProcessPass::RecordTonemap( frameCmd.Get(), gpu, input );

    Vk_PostProcessPassDetail::gSceneColorLayout = Vk_FrameCmd::ImageLayoutToVk( sceneLayout );
    Vk_PostProcessPassDetail::gBloomPingLayout  = Vk_FrameCmd::ImageLayoutToVk( pingLayout );
}

}  // namespace Vk_PostProcessPass

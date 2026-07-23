// Module: Vk_SsrPass — thin loader + Vk mirrors over Gfx_SsrPass Init/Record.
#include "Vk_SsrPass.h"

#include "../Gfx/Gfx_SsrPass.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_DepthPyramidPass.h"
#include "Vk_FrameCmd.h"
#include "Vk_Initializer.h"
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

constexpr char     kSsrShaderPath[] = "VulkanDesktop/Shader_Generated/SsrTrace.spv";
constexpr VkFormat kSsrFormat       = VK_FORMAT_R16G16B16A16_SFLOAT;

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

void ClearVkMirrors( Vk_SsrState& aState ) {
    aState.myComputePipeline     = VK_NULL_HANDLE;
    aState.myPipelineLayout      = VK_NULL_HANDLE;
    aState.myDescriptorSetLayout = VK_NULL_HANDLE;
    aState.myDescriptorPool      = VK_NULL_HANDLE;
    aState.myGBufferSampler      = VK_NULL_HANDLE;
}

void SyncVkMirrors( Vk_Renderer& aCore ) {
    Vk_SsrState& state = aCore.mySsrState;
    Rhi_Device&  rhi   = aCore.myGfxRhiDevice;
    const auto&  gfx   = state.myGfx;

    state.myComputePipeline     = RhiVulkan::PipelineGetVk( rhi, gfx.myPipeline );
    state.myPipelineLayout      = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myLayout );
    state.myDescriptorSetLayout = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.mySetLayout );
    state.myDescriptorPool      = RhiVulkan::DescriptorPoolGetVk( rhi, gfx.myPool );
    state.myGBufferSampler      = RhiVulkan::SamplerGetVk( rhi, gfx.myGBufferSampler );
}

void DestroySsrImage( Vk_Renderer& aCore ) {
    const VkDevice      device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator  allocator = aCore.myRhi.myDeviceCtx.myAllocator;
    Vk_TextureResource& texture   = aCore.mySsrState.mySsrOutput;

    if ( texture.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( device, texture.ImageView(), nullptr );
        texture.ImageView() = VK_NULL_HANDLE;
    }
    if ( texture.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, texture.Image(), texture.Allocation() );
        texture.AllocImage() = {};
    }
    Vk_SsrPassDetail::gSsrLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void DestroySsrImages( Vk_Renderer& aCore ) {
    const VkDevice     device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator allocator = aCore.myRhi.myDeviceCtx.myAllocator;
    for ( Vk_TextureResource& history : aCore.mySsrState.myLitHdrHistory ) {
        if ( history.ImageView() != VK_NULL_HANDLE ) {
            vkDestroyImageView( device, history.ImageView(), nullptr );
            history.ImageView() = VK_NULL_HANDLE;
        }
        if ( history.Image() != VK_NULL_HANDLE ) {
            vmaDestroyImage( allocator, history.Image(), history.Allocation() );
            history.AllocImage() = {};
        }
    }
    Vk_SsrPassDetail::gHistoryLayouts = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };
}

void CreateHistoryImages( Vk_Renderer& aCore ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    for ( uint32_t i = 0; i < 2; ++i ) {
        Vk_TextureResource& history = aCore.mySsrState.myLitHdrHistory[ i ];
        aCore.CreateImage( extent, kSsrFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                           VK_SAMPLE_COUNT_1_BIT, history.AllocImage() );
        history.ImageView() = aCore.CreateImageView( history.Image(), kSsrFormat, VK_IMAGE_ASPECT_COLOR_BIT );
        // First-frame SSR samples history as SHADER_READ_ONLY before RecordHistoryUpdate has written it.
        aCore.TransitionImageLayout( history.Image(), kSsrFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
        Vk_SsrPassDetail::gHistoryLayouts[ i ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    UtilLogger::Info( "SSR", "lit HDR history ping-pong: " + std::to_string( extent.width ) + "x" + std::to_string( extent.height ) );
}

void CreateSsrImage( Vk_Renderer& aCore ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    Vk_TextureResource& texture = aCore.mySsrState.mySsrOutput;
    aCore.CreateImage( extent, kSsrFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, texture.AllocImage() );
    texture.ImageView() = aCore.CreateImageView( texture.Image(), kSsrFormat, VK_IMAGE_ASPECT_COLOR_BIT );

    UtilLogger::Info( "SSR", "target: " + std::to_string( extent.width ) + "x" + std::to_string( extent.height ) + " format=RGBA16F" );
}

void UpdateDescriptorSetImpl( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler     = state.myGBufferSampler;
    normalInfo.imageView   = aCore.myGBufferState.myNormalRoughness.ImageView();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo worldPosInfo{};
    worldPosInfo.sampler     = state.myGBufferSampler;
    worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
    worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo historyInfo{};
    historyInfo.sampler = state.myGBufferSampler;
    // Read buffer written at end of previous frame (RecordHistoryUpdate).
    const uint32_t readIndex = state.myHistoryWriteIndex;
    historyInfo.imageView    = state.myLitHdrHistory[ readIndex ].ImageView();
    historyInfo.imageLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo hiZInfo{};
    hiZInfo.sampler     = aCore.myDepthPyramidState.myPyramidSampler;
    hiZInfo.imageView   = aCore.myDepthPyramidState.myPyramid.ImageView();
    hiZInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.sampler     = state.myGBufferSampler;
    albedoInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo ssrOutInfo{};
    ssrOutInfo.imageView   = state.mySsrOutput.ImageView();
    ssrOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 7 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &historyInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hiZInfo, 4, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoInfo, 5, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ssrOutInfo, 6, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
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
    SyncVkMirrors( aCore );
    UtilLogger::Info( "PIPELINE", "SSR compute pipeline created (Gfx)." );
}

void AllocateDescriptorSets( Vk_Renderer& aCore ) {
    Vk_SsrState& state = aCore.mySsrState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &state.myDescriptorSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_SsrPass: failed to allocate descriptor set" );
        }
        UpdateDescriptorSetImpl( aCore, i );
    }
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
    DestroySsrImage( aCore );
    DestroySsrImages( aCore );
    if ( aCore.myGfxRhiDevice ) {
        Gfx_SsrPass::Destroy( aCore.myGfxRhiDevice, aCore.mySsrState.myGfx );
    }
    aCore.mySsrState.myDescriptorSets    = {};
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    ClearVkMirrors( aCore.mySsrState );
    aCore.mySsrState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroySsrImage( aCore );
    DestroySsrImages( aCore );
    CreateSsrImage( aCore );
    CreateHistoryImages( aCore );
    aCore.mySsrState.myHistoryReady = false;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateDescriptorSetImpl( aCore, i );
    }
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
    CreateSsrImage( aCore );
    CreateHistoryImages( aCore );
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    AllocateDescriptorSets( aCore );
    aCore.mySsrState.myInitialized = true;
}

// RecordCompute / RecordHistoryUpdate via Gfx_SsrPass (Rhi + Vk_FrameCmd).

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || state.myDescriptorSets[ aFrameIndex ] == VK_NULL_HANDLE || !aCore.myGfxRhiDevice ) {
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

    Gfx_SsrPass::GpuResources gpu{};
    gpu.myPipeline = RhiVulkan::PipelineAdopt( state.myComputePipeline );
    gpu.myLayout   = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.mySet      = RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ] );
    gpu.myOutput   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySsrOutput.Image(), state.mySsrOutput.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

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

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myOutput );
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

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_SsrPass::GpuResources gpu{};
    gpu.mySceneColor   = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, sceneColor.Image(), sceneColor.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
    gpu.myHistoryWrite = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, history.Image(), history.ImageView(), VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

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

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySceneColor );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myHistoryWrite );
}

VkImageView GetOutputImageView( const Vk_Renderer& aCore ) {
    return aCore.mySsrState.mySsrOutput.ImageView();
}

}  // namespace Vk_SsrPass

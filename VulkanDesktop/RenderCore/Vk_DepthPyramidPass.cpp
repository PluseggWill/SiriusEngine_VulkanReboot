// Module: Vk_DepthPyramidPass — Hi-Z min-depth pyramid (R32_SFLOAT mip chain) from G-buffer depth.
#include "Vk_DepthPyramidPass.h"

#include "../Gfx/Gfx_DepthPyramidPass.h"

static_assert( kHiZMaxMipLevels == Gfx_DepthPyramidPass::kMaxMipLevels, "Hi-Z mip cap must match Gfx_DepthPyramidPass" );

#include "../Rhi/Rhi_Device.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char     kDepthPyramidShaderPath[] = "VulkanDesktop/Shader_Generated/DepthPyramid.spv";
constexpr VkFormat kPyramidFormat            = VK_FORMAT_R32_SFLOAT;

}  // namespace

namespace Vk_DepthPyramidPassDetail {
VkImageLayout gPyramidLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

namespace {

struct DepthPyramidPushConstants {
    uint32_t mipLevel;
    uint32_t isFirstMip;
    uint32_t srcWidth;
    uint32_t srcHeight;
};

static_assert( sizeof( DepthPyramidPushConstants ) == 16, "DepthPyramidPushConstants must match DepthPyramid.comp push block" );

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

VkImageView CreateMipImageView( Vk_Renderer& aCore, VkImage aImage, VkFormat aFormat, uint32_t aBaseMip ) {
    VkImageViewCreateInfo viewInfo         = VkInit::ImageViewCreateInfo( aFormat, aImage, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
    viewInfo.subresourceRange.baseMipLevel = aBaseMip;
    viewInfo.subresourceRange.levelCount   = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if ( vkCreateImageView( aCore.myRhi.myDeviceCtx.myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create mip image view" );
    }
    return imageView;
}

void DestroyRhiPipelineObjects( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;
    if ( !rhi ) {
        return;
    }
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        for ( uint32_t mip = 0; mip < kHiZMaxMipLevels; ++mip ) {
            state.myRhiSets[ i ][ mip ]        = {};
            state.myDescriptorSets[ i ][ mip ] = VK_NULL_HANDLE;
        }
    }
    Rhi::DeviceDestroyPipeline( rhi, state.myRhiPipeline );
    Rhi::DeviceDestroyPipelineLayout( rhi, state.myRhiLayout );
    Rhi::DeviceDestroyDescriptorPool( rhi, state.myRhiPool );
    Rhi::DeviceDestroyDescriptorSetLayout( rhi, state.myRhiSetLayout );
    Rhi::DeviceDestroySampler( rhi, state.myRhiDepthSampler );
    Rhi::DeviceDestroySampler( rhi, state.myRhiPyramidSampler );
    state.myComputePipeline     = VK_NULL_HANDLE;
    state.myPipelineLayout      = VK_NULL_HANDLE;
    state.myDescriptorSetLayout = VK_NULL_HANDLE;
    state.myDescriptorPool      = VK_NULL_HANDLE;
    state.myDepthSampler        = VK_NULL_HANDLE;
    state.myPyramidSampler      = VK_NULL_HANDLE;
}

void DestroyPyramidImage( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state     = aCore.myDepthPyramidState;
    const VkDevice        device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator    allocator = aCore.myRhi.myDeviceCtx.myAllocator;

    // Sample view is a full mip-chain view, distinct from per-mip storage views.
    VkImageView sampleView      = state.myPyramid.ImageView();
    bool        sampleIsMipView = false;
    for ( VkImageView& view : state.myMipViews ) {
        if ( view != VK_NULL_HANDLE && view == sampleView ) {
            sampleIsMipView = true;
        }
    }
    if ( sampleView != VK_NULL_HANDLE && !sampleIsMipView ) {
        vkDestroyImageView( device, sampleView, nullptr );
    }
    state.myPyramid.ImageView() = VK_NULL_HANDLE;

    for ( VkImageView& view : state.myMipViews ) {
        if ( view != VK_NULL_HANDLE ) {
            vkDestroyImageView( device, view, nullptr );
            view = VK_NULL_HANDLE;
        }
    }

    if ( state.myPyramid.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, state.myPyramid.Image(), state.myPyramid.Allocation() );
        state.myPyramid.AllocImage() = {};
    }

    Vk_DepthPyramidPassDetail::gPyramidLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    state.myMipLevelCount                     = 0;
}

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, uint32_t aDstMip, uint32_t aSrcMip ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;

    Rhi_Texture depthTex = RhiVulkan::TextureAdopt( rhi, aCore.myGBufferState.myDepth.Image(), aCore.myGBufferState.myDepth.ImageView(), VK_FORMAT_D32_SFLOAT, 1 );
    Rhi_Texture srcTex   = RhiVulkan::TextureAdopt( rhi, state.myPyramid.Image(), state.myMipViews[ aSrcMip ], kPyramidFormat, 1 );
    Rhi_Texture dstTex   = RhiVulkan::TextureAdopt( rhi, state.myPyramid.Image(), state.myMipViews[ aDstMip ], kPyramidFormat, 1 );

    std::array< Rhi::DescriptorImageWrite, 3 > writes{};
    writes[ 0 ].mySet     = state.myRhiSets[ aFrameIndex ][ aDstMip ];
    writes[ 0 ].myBinding = 0;
    writes[ 0 ].myType    = Rhi_DescriptorType::CombinedImageSampler;
    writes[ 0 ].mySampler = state.myRhiDepthSampler;
    writes[ 0 ].myTexture = depthTex;
    writes[ 0 ].myLayout  = Rhi_ImageLayout::DepthStencilReadOnly;

    writes[ 1 ].mySet     = state.myRhiSets[ aFrameIndex ][ aDstMip ];
    writes[ 1 ].myBinding = 1;
    writes[ 1 ].myType    = Rhi_DescriptorType::StorageImage;
    writes[ 1 ].myTexture = srcTex;
    writes[ 1 ].myLayout  = Rhi_ImageLayout::General;

    writes[ 2 ].mySet     = state.myRhiSets[ aFrameIndex ][ aDstMip ];
    writes[ 2 ].myBinding = 2;
    writes[ 2 ].myType    = Rhi_DescriptorType::StorageImage;
    writes[ 2 ].myTexture = dstTex;
    writes[ 2 ].myLayout  = Rhi_ImageLayout::General;

    Rhi::DeviceUpdateDescriptorImages( rhi, writes.data(), static_cast< uint32_t >( writes.size() ) );

    Rhi::DeviceDestroyTexture( rhi, depthTex );
    Rhi::DeviceDestroyTexture( rhi, srcTex );
    Rhi::DeviceDestroyTexture( rhi, dstTex );
}

void CreatePyramidImage( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    state.myMipLevelCount = Gfx_ComputeHiZMipLevelCount( width, height, kHiZMaxMipLevels );
    state.myMipLevelCount = std::min( state.myMipLevelCount, kHiZMaxMipLevels );

    const VkExtent2D extent{ width, height };
    aCore.CreateImage( extent, kPyramidFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                       state.myMipLevelCount, VK_SAMPLE_COUNT_1_BIT, state.myPyramid.AllocImage() );

    for ( uint32_t mip = 0; mip < state.myMipLevelCount; ++mip ) {
        state.myMipViews[ mip ] = CreateMipImageView( aCore, state.myPyramid.Image(), kPyramidFormat, mip );
    }

    // Full mip-chain view for SSR / deferred debug (textureLod). Per-mip views stay for storage writes.
    VkImageViewCreateInfo sampleViewInfo = VkInit::ImageViewCreateInfo( kPyramidFormat, state.myPyramid.Image(), VK_IMAGE_ASPECT_COLOR_BIT, state.myMipLevelCount );
    VkImageView           sampleView     = VK_NULL_HANDLE;
    if ( vkCreateImageView( aCore.myRhi.myDeviceCtx.myDevice, &sampleViewInfo, nullptr, &sampleView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create sample mip-chain view" );
    }
    state.myPyramid.ImageView() = sampleView;

    UtilLogger::Info( "HIZ", "Depth pyramid: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " mips=" + std::to_string( state.myMipLevelCount )
                                 + " (mip0=full-res copy)" );
}

void AllocateDescriptorSets( Vk_Renderer& aCore, bool aAllocateDescriptors ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        for ( uint32_t mip = 0; mip < kHiZMaxMipLevels; ++mip ) {
            if ( aAllocateDescriptors ) {
                state.myRhiSets[ i ][ mip ]        = Rhi::DeviceAllocateDescriptorSet( rhi, state.myRhiPool, state.myRhiSetLayout );
                state.myDescriptorSets[ i ][ mip ] = RhiVulkan::DescriptorSetGetVk( rhi, state.myRhiSets[ i ][ mip ] );
                if ( !state.myRhiSets[ i ][ mip ] ) {
                    throw std::runtime_error( "Vk_DepthPyramidPass: failed to allocate descriptor set" );
                }
            }
            if ( state.myMipLevelCount > 0 && mip < state.myMipLevelCount ) {
                const uint32_t srcMip = mip > 0 ? mip - 1 : 0;
                UpdateDescriptorSet( aCore, i, mip, srcMip );
            }
        }
    }
}

void CreatePipeline( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: myGfxRhiDevice required for Rhi create" );
    }

    Vk_DepthPyramidState&     state   = aCore.myDepthPyramidState;
    Rhi_Device&               rhi     = aCore.myGfxRhiDevice;
    const std::string         spvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDepthPyramidShaderPath );
    const std::vector< char > spirv   = LoadSpirvBytes( spvPath );

    Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( rhi, spirv.data(), spirv.size() );
    if ( !shader ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create shader module" );
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 3 > bindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           setLayoutDesc{};
    setLayoutDesc.myBindings     = bindings.data();
    setLayoutDesc.myBindingCount = static_cast< uint32_t >( bindings.size() );
    state.myRhiSetLayout         = Rhi::DeviceCreateDescriptorSetLayout( rhi, setLayoutDesc );
    if ( !state.myRhiSetLayout ) {
        Rhi::DeviceDestroyShaderModule( rhi, shader );
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create descriptor set layout" );
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Compute;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = sizeof( DepthPyramidPushConstants );

    Rhi::PipelineLayoutDesc layoutDesc{};
    layoutDesc.mySetLayouts     = &state.myRhiSetLayout;
    layoutDesc.mySetLayoutCount = 1;
    layoutDesc.myPushRanges     = &push;
    layoutDesc.myPushRangeCount = 1;
    state.myRhiLayout           = Rhi::DeviceCreatePipelineLayout( rhi, layoutDesc );
    if ( !state.myRhiLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( rhi, state.myRhiSetLayout );
        Rhi::DeviceDestroyShaderModule( rhi, shader );
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create pipeline layout" );
    }

    Rhi::ComputePipelineDesc pipeDesc{};
    pipeDesc.myShader   = shader;
    pipeDesc.myLayout   = state.myRhiLayout;
    state.myRhiPipeline = Rhi::DeviceCreateComputePipeline( rhi, pipeDesc );
    Rhi::DeviceDestroyShaderModule( rhi, shader );
    if ( !state.myRhiPipeline ) {
        Rhi::DeviceDestroyPipelineLayout( rhi, state.myRhiLayout );
        Rhi::DeviceDestroyDescriptorSetLayout( rhi, state.myRhiSetLayout );
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create compute pipeline" );
    }

    Rhi::SamplerDesc depthSamplerDesc{};
    depthSamplerDesc.myMagFilter = Rhi_Filter::Nearest;
    depthSamplerDesc.myMinFilter = Rhi_Filter::Nearest;
    state.myRhiDepthSampler      = Rhi::DeviceCreateSampler( rhi, depthSamplerDesc );

    Rhi::SamplerDesc pyramidSamplerDesc{};
    pyramidSamplerDesc.myMagFilter  = Rhi_Filter::Nearest;
    pyramidSamplerDesc.myMinFilter  = Rhi_Filter::Nearest;
    pyramidSamplerDesc.myMipmapMode = Rhi_MipmapMode::Nearest;
    pyramidSamplerDesc.myMaxLod     = static_cast< float >( kHiZMaxMipLevels );
    state.myRhiPyramidSampler       = Rhi::DeviceCreateSampler( rhi, pyramidSamplerDesc );
    if ( !state.myRhiDepthSampler || !state.myRhiPyramidSampler ) {
        DestroyRhiPipelineObjects( aCore );
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create samplers" );
    }

    const std::array< Rhi::DescriptorPoolSize, 2 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels },
        { Rhi_DescriptorType::StorageImage, MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels * 2 },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    state.myRhiPool          = Rhi::DeviceCreateDescriptorPool( rhi, poolDesc );
    if ( !state.myRhiPool ) {
        DestroyRhiPipelineObjects( aCore );
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create descriptor pool" );
    }

    state.myComputePipeline     = RhiVulkan::PipelineGetVk( rhi, state.myRhiPipeline );
    state.myPipelineLayout      = RhiVulkan::PipelineLayoutGetVk( rhi, state.myRhiLayout );
    state.myDescriptorSetLayout = RhiVulkan::DescriptorSetLayoutGetVk( rhi, state.myRhiSetLayout );
    state.myDescriptorPool      = RhiVulkan::DescriptorPoolGetVk( rhi, state.myRhiPool );
    state.myDepthSampler        = RhiVulkan::SamplerGetVk( rhi, state.myRhiDepthSampler );
    state.myPyramidSampler      = RhiVulkan::SamplerGetVk( rhi, state.myRhiPyramidSampler );

    UtilLogger::Info( "PIPELINE", "DepthPyramid compute pipeline created (Rhi)." );
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
    DestroyPyramidImage( aCore );
    DestroyRhiPipelineObjects( aCore );
    aCore.myDepthPyramidState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myDepthPyramidState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroyPyramidImage( aCore );
    CreatePyramidImage( aCore );
    AllocateDescriptorSets( aCore, false );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myDepthPyramidState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_DepthPyramidPass::Init (Rhi create)." );
    CreatePipeline( aCore );
    CreatePyramidImage( aCore );
    AllocateDescriptorSets( aCore, true );
    aCore.myDepthPyramidState.myInitialized = true;
}

VkImageView GetMipView( const Vk_Renderer& aCore, uint32_t aMipLevel ) {
    if ( aMipLevel >= aCore.myDepthPyramidState.myMipLevelCount || aMipLevel >= kHiZMaxMipLevels ) {
        return VK_NULL_HANDLE;
    }
    return aCore.myDepthPyramidState.myMipViews[ aMipLevel ];
}

uint32_t GetMipLevelCount( const Vk_Renderer& aCore ) {
    return aCore.myDepthPyramidState.myMipLevelCount;
}

}  // namespace Vk_DepthPyramidPass

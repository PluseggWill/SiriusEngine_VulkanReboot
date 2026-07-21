// Module: Vk_DepthPyramidPass — Hi-Z min-depth pyramid (R32_SFLOAT mip chain) from G-buffer depth.
#include "Vk_DepthPyramidPass.h"

#include "../Gfx/Gfx_DepthPyramidPass.h"

static_assert( kHiZMaxMipLevels == Gfx_DepthPyramidPass::kMaxMipLevels, "Hi-Z mip cap must match Gfx_DepthPyramidPass" );

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

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

void DestroyPyramidImage( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state     = aCore.myDepthPyramidState;
    const VkDevice        device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator    allocator = aCore.myRhi.myDeviceCtx.myAllocator;

    for ( VkImageView& view : state.myMipViews ) {
        if ( view != VK_NULL_HANDLE ) {
            vkDestroyImageView( device, view, nullptr );
            view = VK_NULL_HANDLE;
        }
    }

    state.myPyramid.ImageView() = VK_NULL_HANDLE;

    if ( state.myPyramid.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, state.myPyramid.Image(), state.myPyramid.Allocation() );
        state.myPyramid.AllocImage() = {};
    }

    Vk_DepthPyramidPassDetail::gPyramidLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    state.myMipLevelCount                     = 0;
}

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, uint32_t aDstMip, uint32_t aSrcMip ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myDepthSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo srcInfo{};
    srcInfo.imageView   = state.myMipViews[ aSrcMip ];
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = state.myMipViews[ aDstMip ];
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 3 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ][ aDstMip ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ][ aDstMip ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ][ aDstMip ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, 2, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
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
    state.myPyramid.ImageView() = state.myMipViews[ 0 ];

    UtilLogger::Info( "HIZ", "Depth pyramid: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " mips=" + std::to_string( state.myMipLevelCount ) );
}

void AllocateDescriptorSets( Vk_Renderer& aCore, bool aAllocateDescriptors ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        for ( uint32_t mip = 0; mip < kHiZMaxMipLevels; ++mip ) {
            if ( aAllocateDescriptors ) {
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool     = state.myDescriptorPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts        = &state.myDescriptorSetLayout;
                if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ][ mip ] ) != VK_SUCCESS ) {
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
    Vk_DepthPyramidState& state         = aCore.myDepthPyramidState;
    const std::string     spvPath       = UtilLoader::ResolvePath( aCore.EngineConfig(), kDepthPyramidShaderPath );
    VkShaderModule        computeModule = aCore.CreateShaderModule( spvPath );

    const std::array< VkDescriptorSetLayoutBinding, 3 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myDescriptorSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create descriptor set layout" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( DepthPyramidPushConstants );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.myDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = state.myPipelineLayout;
    if ( vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &state.myComputePipeline )
         != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, computeModule, nullptr );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_NEAREST;
    samplerInfo.minFilter               = VK_FILTER_NEAREST;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myDepthSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create depth sampler" );
    }

    VkSamplerCreateInfo pyramidSamplerInfo{};
    pyramidSamplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    pyramidSamplerInfo.magFilter    = VK_FILTER_NEAREST;
    pyramidSamplerInfo.minFilter    = VK_FILTER_NEAREST;
    pyramidSamplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    pyramidSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pyramidSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pyramidSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pyramidSamplerInfo.maxLod       = static_cast< float >( kHiZMaxMipLevels );
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &pyramidSamplerInfo, nullptr, &state.myPyramidSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create pyramid sampler" );
    }

    std::array< VkDescriptorPoolSize, 2 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels * 2 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT * kHiZMaxMipLevels;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create descriptor pool" );
    }

    const VkDevice              device          = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipeline            computePipeline = state.myComputePipeline;
    const VkPipelineLayout      pipelineLayout  = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout       = state.myDescriptorSetLayout;
    const VkDescriptorPool      descriptorPool  = state.myDescriptorPool;
    const VkSampler             depthSampler    = state.myDepthSampler;
    const VkSampler             pyramidSampler  = state.myPyramidSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, computePipeline, pipelineLayout, setLayout, descriptorPool, depthSampler, pyramidSampler ]() {
        if ( computePipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( device, computePipeline, nullptr );
        }
        if ( pipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        }
        if ( setLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, setLayout, nullptr );
        }
        if ( descriptorPool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        }
        if ( depthSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, depthSampler, nullptr );
        }
        if ( pyramidSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, pyramidSampler, nullptr );
        }
    } );

    UtilLogger::Info( "PIPELINE", "DepthPyramid compute pipeline created." );
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
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        for ( uint32_t mip = 0; mip < kHiZMaxMipLevels; ++mip ) {
            aCore.myDepthPyramidState.myDescriptorSets[ i ][ mip ] = VK_NULL_HANDLE;
        }
    }
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
    UtilLogger::Info( "FG", "Vk_DepthPyramidPass::Init." );
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

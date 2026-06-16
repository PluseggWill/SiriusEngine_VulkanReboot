// Module: Vk_DepthPyramidPass — Hi-Z min-depth pyramid (R32_SFLOAT mip chain) from G-buffer depth.
#include "Vk_DepthPyramidPass.h"

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

VkImageLayout sPyramidLayout = VK_IMAGE_LAYOUT_UNDEFINED;

struct DepthPyramidPushConstants {
    uint32_t mipLevel;
    uint32_t isFirstMip;
    uint32_t srcWidth;
    uint32_t srcHeight;
};

static_assert( sizeof( DepthPyramidPushConstants ) == 16, "DepthPyramidPushConstants must match DepthPyramid.comp push block" );

VkImageMemoryBarrier DepthImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess,
                                        uint32_t aBaseMip, uint32_t aMipCount ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, aBaseMip, aMipCount, 0, 1 };
    return barrier;
}

VkImageView CreateMipImageView( Vk_Renderer& aCore, VkImage aImage, VkFormat aFormat, uint32_t aBaseMip ) {
    VkImageViewCreateInfo viewInfo         = VkInit::ImageViewCreateInfo( aFormat, aImage, VK_IMAGE_ASPECT_COLOR_BIT, 1 );
    viewInfo.subresourceRange.baseMipLevel = aBaseMip;
    viewInfo.subresourceRange.levelCount   = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if ( vkCreateImageView( aCore.myDeviceCtx.myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create mip image view" );
    }
    return imageView;
}

void DestroyPyramidImage( Vk_Renderer& aCore ) {
    Vk_DepthPyramidState& state     = aCore.myDepthPyramidState;
    const VkDevice        device    = aCore.myDeviceCtx.myDevice;
    const VmaAllocator    allocator = aCore.myDeviceCtx.myAllocator;

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

    sPyramidLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    state.myMipLevelCount = 0;
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
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
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
                if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ][ mip ] ) != VK_SUCCESS ) {
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
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myDescriptorSetLayout ) != VK_SUCCESS ) {
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
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = state.myPipelineLayout;
    if ( vkCreateComputePipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &state.myComputePipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, computeModule, nullptr );

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
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myDepthSampler ) != VK_SUCCESS ) {
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
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &pyramidSamplerInfo, nullptr, &state.myPyramidSampler ) != VK_SUCCESS ) {
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
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DepthPyramidPass: failed to create descriptor pool" );
    }

    const VkDevice              device          = aCore.myDeviceCtx.myDevice;
    const VkPipeline            computePipeline = state.myComputePipeline;
    const VkPipelineLayout      pipelineLayout  = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout       = state.myDescriptorSetLayout;
    const VkDescriptorPool      descriptorPool  = state.myDescriptorPool;
    const VkSampler             depthSampler    = state.myDepthSampler;
    const VkSampler             pyramidSampler  = state.myPyramidSampler;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, computePipeline, pipelineLayout, setLayout, descriptorPool, depthSampler, pyramidSampler ]() {
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

void CmdBarrierPyramidForComputeWrite( VkCommandBuffer aCommandBuffer, VkImage aImage, uint32_t aMipCount, VkImageLayout& aInOutLayout ) {
    const VkImageLayout  oldLayout = aInOutLayout;
    const VkImageLayout  newLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageMemoryBarrier barrier =
        DepthImageBarrier( aImage, oldLayout, newLayout, oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT, 0, aMipCount );
    const VkPipelineStageFlags srcStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    aInOutLayout = newLayout;
}

}  // namespace

namespace Vk_DepthPyramidPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myDepthPyramidState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
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
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
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

void RecordBuild( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DepthPyramidState& state = aCore.myDepthPyramidState;
    if ( !state.myInitialized || state.myMipLevelCount == 0 || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;

    CmdBarrierPyramidForComputeWrite( aCommandBuffer, state.myPyramid.Image(), state.myMipLevelCount, sPyramidLayout );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myComputePipeline );

    for ( uint32_t dstMip = 0; dstMip < state.myMipLevelCount; ++dstMip ) {
        const bool     isFirstMip = ( dstMip == 0 );
        const uint32_t srcMip     = dstMip > 0 ? dstMip - 1 : 0;
        const uint32_t srcWidth   = isFirstMip ? width : std::max( 1u, width >> dstMip );
        const uint32_t srcHeight  = isFirstMip ? height : std::max( 1u, height >> dstMip );
        const uint32_t dstWidth   = std::max( 1u, srcWidth >> 1 );
        const uint32_t dstHeight  = std::max( 1u, srcHeight >> 1 );

        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myPipelineLayout, 0, 1, &state.myDescriptorSets[ aFrameIndex ][ dstMip ], 0, nullptr );

        DepthPyramidPushConstants push{};
        push.mipLevel   = dstMip;
        push.isFirstMip = isFirstMip ? 1u : 0u;
        push.srcWidth   = srcWidth;
        push.srcHeight  = srcHeight;
        vkCmdPushConstants( aCommandBuffer, state.myPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

        if ( dstMip > 0 ) {
            VkImageMemoryBarrier srcReady = DepthImageBarrier( state.myPyramid.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                                                               VK_ACCESS_SHADER_READ_BIT, srcMip, 1 );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &srcReady );
        }

        if ( aCore.AreCommandDebugLabelsEnabled() ) {
            aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=DepthPyramidMip" );
        }
        vkCmdDispatch( aCommandBuffer, ( dstWidth + 7 ) / 8, ( dstHeight + 7 ) / 8, 1 );
        if ( aCore.AreCommandDebugLabelsEnabled() ) {
            aCore.CmdEndDebugLabel( aCommandBuffer );
        }

        VkImageMemoryBarrier dstWritten = DepthImageBarrier( state.myPyramid.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                                                             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, dstMip, 1 );
        vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &dstWritten );
    }

    VkImageMemoryBarrier pyramidForSample = DepthImageBarrier( state.myPyramid.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                               VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0, state.myMipLevelCount );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &pyramidForSample );
    sPyramidLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

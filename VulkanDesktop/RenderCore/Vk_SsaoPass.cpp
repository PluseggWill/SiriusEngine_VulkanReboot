// Module: Vk_SsaoPass — screen-space ambient occlusion (compute + separable blur).
#include "Vk_SsaoPass.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_Core.h"
#include "Vk_Initializer.h"

#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace {

constexpr char     kSsaoShaderPath[]     = "VulkanDesktop/Shader_Generated/Ssao.spv";
constexpr char     kSsaoBlurShaderPath[] = "VulkanDesktop/Shader_Generated/SsaoBlur.spv";
constexpr VkFormat kAoFormat             = VK_FORMAT_R8_UNORM;

VkImageLayout sAoRawLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout sAoBlurLayout = VK_IMAGE_LAYOUT_UNDEFINED;

struct SsaoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 viewProj;
    alignas( 16 ) glm::vec4 params;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( SsaoPushConstants ) == 224, "SsaoPushConstants must match Ssao.comp push block" );

struct SsaoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( SsaoBlurPushConstants ) == 8, "SsaoBlurPushConstants must match SsaoBlur.comp push block" );

VkImageMemoryBarrier ColorImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return barrier;
}

VkImageMemoryBarrier DepthImageBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    return barrier;
}

void DestroyAoTexture( Vk_Core& aCore, Gfx_Texture& aTexture ) {
    const VkDevice     device    = aCore.myDeviceCtx.myDevice;
    const VmaAllocator allocator = aCore.myDeviceCtx.myAllocator;
    if ( aTexture.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( device, aTexture.ImageView(), nullptr );
        aTexture.ImageView() = VK_NULL_HANDLE;
    }
    if ( aTexture.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, aTexture.Image(), aTexture.Allocation() );
        aTexture.AllocImage() = {};
    }
}

void DestroyAoImages( Vk_Core& aCore ) {
    DestroyAoTexture( aCore, aCore.mySsaoState.myAoRaw );
    DestroyAoTexture( aCore, aCore.mySsaoState.myAoBlur );
    sAoRawLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    sAoBlurLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void CreateAoImage( Vk_Core& aCore, Gfx_Texture& aTexture ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    aCore.CreateImage( extent, kAoFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
    aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), kAoFormat, VK_IMAGE_ASPECT_COLOR_BIT );
}

void CreateAoImages( Vk_Core& aCore ) {
    CreateAoImage( aCore, aCore.mySsaoState.myAoRaw );
    CreateAoImage( aCore, aCore.mySsaoState.myAoBlur );

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    UtilLogger::Info( "SSAO", "AO targets: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=R8_UNORM" );
}

void UpdateSsaoDescriptorSet( Vk_Core& aCore, uint32_t aFrameIndex ) {
    Vk_SsaoState& state = aCore.mySsaoState;

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

    VkDescriptorImageInfo aoOutInfo{};
    aoOutInfo.imageView   = state.myAoRaw.ImageView();
    aoOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.mySsaoDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.mySsaoDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.mySsaoDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.mySsaoDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &aoOutInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBlurDescriptorSet( Vk_Core& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aSrcView, VkImageView aDstView ) {
    VkDescriptorImageInfo srcInfo{};
    srcInfo.imageView   = aSrcView;
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = aDstView;
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateAllDescriptorSets( Vk_Core& aCore ) {
    Vk_SsaoState& state = aCore.mySsaoState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateSsaoDescriptorSet( aCore, i );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurHorizDescriptorSets[ i ], state.myAoRaw.ImageView(), state.myAoBlur.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurVertDescriptorSets[ i ], state.myAoBlur.ImageView(), state.myAoRaw.ImageView() );
    }
}

void AllocateDescriptorSets( Vk_Core& aCore, bool aAllocateDescriptors ) {
    Vk_SsaoState& state = aCore.mySsaoState;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        if ( aAllocateDescriptors ) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = state.myDescriptorPool;
            allocInfo.descriptorSetCount = 1;

            allocInfo.pSetLayouts = &state.mySsaoSetLayout;
            if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.mySsaoDescriptorSets[ i ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "Vk_SsaoPass: failed to allocate SSAO descriptor set" );
            }

            allocInfo.pSetLayouts = &state.myBlurSetLayout;
            if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myBlurHorizDescriptorSets[ i ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "Vk_SsaoPass: failed to allocate horizontal blur descriptor set" );
            }
            if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myBlurVertDescriptorSets[ i ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "Vk_SsaoPass: failed to allocate vertical blur descriptor set" );
            }
        }
    }
    UpdateAllDescriptorSets( aCore );
}

VkPipeline CreateComputePipeline( Vk_Core& aCore, const std::string& aSpvPath, VkDescriptorSetLayout aSetLayout, VkPipelineLayout& aOutLayout,
                                  VkPushConstantRange aPushRange ) {
    VkShaderModule computeModule = aCore.CreateShaderModule( aSpvPath );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &aSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &aPushRange;
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &aOutLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = aOutLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if ( vkCreateComputePipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, computeModule, nullptr );
    return pipeline;
}

void CreatePipelines( Vk_Core& aCore ) {
    Vk_SsaoState& state = aCore.mySsaoState;

    const std::array< VkDescriptorSetLayoutBinding, 4 > ssaoBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
    };

    VkDescriptorSetLayoutCreateInfo ssaoLayoutInfo{};
    ssaoLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ssaoLayoutInfo.bindingCount = static_cast< uint32_t >( ssaoBindings.size() );
    ssaoLayoutInfo.pBindings    = ssaoBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &ssaoLayoutInfo, nullptr, &state.mySsaoSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create SSAO descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 2 > blurBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
    };

    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast< uint32_t >( blurBindings.size() );
    blurLayoutInfo.pBindings    = blurBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &blurLayoutInfo, nullptr, &state.myBlurSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create blur descriptor set layout" );
    }

    VkPushConstantRange ssaoPushRange{};
    ssaoPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ssaoPushRange.offset     = 0;
    ssaoPushRange.size       = sizeof( SsaoPushConstants );

    const std::string ssaoSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kSsaoShaderPath );
    state.mySsaoPipeline          = CreateComputePipeline( aCore, ssaoSpvPath, state.mySsaoSetLayout, state.mySsaoPipelineLayout, ssaoPushRange );

    VkPushConstantRange blurPushRange{};
    blurPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    blurPushRange.offset     = 0;
    blurPushRange.size       = sizeof( SsaoBlurPushConstants );

    const std::string blurSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kSsaoBlurShaderPath );
    state.myBlurPipeline          = CreateComputePipeline( aCore, blurSpvPath, state.myBlurSetLayout, state.myBlurPipelineLayout, blurPushRange );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create G-buffer sampler" );
    }

    std::array< VkDescriptorPoolSize, 2 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 5 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT * 3;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsaoPass: failed to create descriptor pool" );
    }

    const VkDevice              device             = aCore.myDeviceCtx.myDevice;
    const VkPipeline            ssaoPipeline       = state.mySsaoPipeline;
    const VkPipeline            blurPipeline       = state.myBlurPipeline;
    const VkPipelineLayout      ssaoPipelineLayout = state.mySsaoPipelineLayout;
    const VkPipelineLayout      blurPipelineLayout = state.myBlurPipelineLayout;
    const VkDescriptorSetLayout ssaoSetLayout      = state.mySsaoSetLayout;
    const VkDescriptorSetLayout blurSetLayout      = state.myBlurSetLayout;
    const VkDescriptorPool      descriptorPool     = state.myDescriptorPool;
    const VkSampler             gbufferSampler     = state.myGBufferSampler;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction(
        [ device, ssaoPipeline, blurPipeline, ssaoPipelineLayout, blurPipelineLayout, ssaoSetLayout, blurSetLayout, descriptorPool, gbufferSampler ]() {
            if ( ssaoPipeline != VK_NULL_HANDLE ) {
                vkDestroyPipeline( device, ssaoPipeline, nullptr );
            }
            if ( blurPipeline != VK_NULL_HANDLE ) {
                vkDestroyPipeline( device, blurPipeline, nullptr );
            }
            if ( ssaoPipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, ssaoPipelineLayout, nullptr );
            }
            if ( blurPipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, blurPipelineLayout, nullptr );
            }
            if ( ssaoSetLayout != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device, ssaoSetLayout, nullptr );
            }
            if ( blurSetLayout != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device, blurSetLayout, nullptr );
            }
            if ( descriptorPool != VK_NULL_HANDLE ) {
                vkDestroyDescriptorPool( device, descriptorPool, nullptr );
            }
            if ( gbufferSampler != VK_NULL_HANDLE ) {
                vkDestroySampler( device, gbufferSampler, nullptr );
            }
        } );

    UtilLogger::Info( "PIPELINE", "SSAO + blur compute pipelines created." );
}

void CmdBarrierGBufferForSsaoRead( Vk_Core& aCore, VkCommandBuffer aCommandBuffer ) {
    std::array< VkImageMemoryBarrier, 2 > barriers = {
        ColorImageBarrier( aCore.myGBufferState.myNormalRoughness.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
        ColorImageBarrier( aCore.myGBufferState.myWorldPosition.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( barriers.size() ), barriers.data() );
}

void CmdBarrierAoForComputeWrite( VkCommandBuffer aCommandBuffer, VkImage aImage, VkImageLayout& aInOutLayout ) {
    const VkImageLayout  oldLayout = aInOutLayout;
    const VkImageLayout  newLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkImageMemoryBarrier barrier =
        ColorImageBarrier( aImage, oldLayout, newLayout, oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT );
    const VkPipelineStageFlags srcStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED                  ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                          : oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                                                                  : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    aInOutLayout = newLayout;
}

void CmdDispatchBlur( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, VkPipeline aPipeline, VkPipelineLayout aLayout, VkDescriptorSet aSet, uint32_t aAxisX, uint32_t aAxisY,
                      uint32_t aWidth, uint32_t aHeight, const char* aDebugLabel ) {
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, aPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, aLayout, 0, 1, &aSet, 0, nullptr );

    SsaoBlurPushConstants push{};
    push.axisX = aAxisX;
    push.axisY = aAxisY;
    vkCmdPushConstants( aCommandBuffer, aLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, aDebugLabel );
    }
    vkCmdDispatch( aCommandBuffer, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

}  // namespace

namespace Vk_SsaoPass {

void Destroy( Vk_Core& aCore ) {
    if ( !aCore.mySsaoState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroyAoImages( aCore );
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        aCore.mySsaoState.mySsaoDescriptorSets[ i ]      = VK_NULL_HANDLE;
        aCore.mySsaoState.myBlurHorizDescriptorSets[ i ] = VK_NULL_HANDLE;
        aCore.mySsaoState.myBlurVertDescriptorSets[ i ]  = VK_NULL_HANDLE;
    }
    aCore.mySsaoState.myInitialized = false;
}

void RecreateForExtent( Vk_Core& aCore ) {
    if ( !aCore.mySsaoState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroyAoImages( aCore );
    CreateAoImages( aCore );
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Core& aCore ) {
    if ( aCore.mySsaoState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_SsaoPass::Init." );
    CreatePipelines( aCore );
    CreateAoImages( aCore );
    AllocateDescriptorSets( aCore, true );
    aCore.mySsaoState.myInitialized = true;
}

void RecordCompute( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_SsaoState& state = aCore.mySsaoState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    CmdBarrierGBufferForSsaoRead( aCore, aCommandBuffer );

    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoRaw.Image(), sAoRawLayout );
    CmdBarrierAoForComputeWrite( aCommandBuffer, state.myAoBlur.Image(), sAoBlurLayout );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.mySsaoPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.mySsaoPipelineLayout, 0, 1, &state.mySsaoDescriptorSets[ aFrameIndex ], 0, nullptr );

    const Gfx_AoSettings& ao = aCore.myAoSettings;
    SsaoPushConstants     push{};
    push.view       = aCore.myCamera.myView;
    push.proj       = aCore.myCamera.myProj;
    push.viewProj   = aCore.myCamera.myProj * aCore.myCamera.myView;
    push.params     = glm::vec4( ao.myRadius, ao.myBias, ao.myPower, ao.myEnabled ? 1.0f : 0.0f );
    push.screenSize = glm::vec2( static_cast< float >( width ), static_cast< float >( height ) );
    push.pad        = glm::vec2( 0.0f );
    vkCmdPushConstants( aCommandBuffer, state.mySsaoPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=SSAO" );
    }
    vkCmdDispatch( aCommandBuffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    VkImageMemoryBarrier rawWritten =
        ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rawWritten );

    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurPipeline, state.myBlurPipelineLayout, state.myBlurHorizDescriptorSets[ aFrameIndex ], 1, 0, width, height,
                     "Pass=SSAOBlurH" );

    VkImageMemoryBarrier blurWritten =
        ColorImageBarrier( state.myAoBlur.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &blurWritten );

    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurPipeline, state.myBlurPipelineLayout, state.myBlurVertDescriptorSets[ aFrameIndex ], 0, 1, width, height,
                     "Pass=SSAOBlurV" );

    // Final blurred AO lives in myAoRaw (horizontal raw->blur, vertical blur->raw).
    VkImageMemoryBarrier aoForDeferred =
        ColorImageBarrier( state.myAoRaw.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &aoForDeferred );
    sAoRawLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

}  // namespace Vk_SsaoPass

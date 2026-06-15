// Module: Vk_ShadowAoSoftPass — pack raw AO + screen-space sun shadow into RG8, bilateral blur.
// Runs after Vk_AoPass; deferred reads soft ping via GetDeferredContactMapView().#include "Vk_ShadowAoSoftPass.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_AoPass.h"
#include "Vk_Core.h"
#include "Vk_Initializer.h"
#include "Vk_ShadowMapPass.h"

#include <vma/vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace {

constexpr char     kPackShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoPack.spv";
constexpr char     kBlurShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoBlur.spv";
constexpr VkFormat kSoftFormat       = VK_FORMAT_R8G8_UNORM;

VkImageLayout sSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout sSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;

struct ShadowAoPackPushConstants {
    alignas( 16 ) glm::vec4 params;
};

static_assert( sizeof( ShadowAoPackPushConstants ) == 16, "ShadowAoPackPushConstants must match ShadowAoPack.comp push block" );

struct ShadowAoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
    float    radius;
    float    depthSigma;
};

static_assert( sizeof( ShadowAoBlurPushConstants ) == 16, "ShadowAoBlurPushConstants must match ShadowAoBlur.comp push block" );

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

void DestroySoftTexture( Vk_Core& aCore, Gfx_Texture& aTexture ) {
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

void DestroySoftImages( Vk_Core& aCore ) {
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.mySoftPing );
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.mySoftPong );
    sSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    sSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void DestroyFallbackImages( Vk_Core& aCore ) {
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.myFallbackAo );
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.myFallbackContact );
}

void CreateFallbackImages( Vk_Core& aCore ) {
    Vk_ShadowAoSoftState& state   = aCore.myShadowAoSoftState;
    const VmaAllocator    allocator = aCore.myDeviceCtx.myAllocator;
    const VkExtent2D      one{ 1, 1 };

    auto uploadIdentity = [&]( VkFormat aFormat, Gfx_Texture& aOut ) {
        aCore.CreateImage( one, aFormat, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT,
                           aOut.AllocImage() );
        aOut.ImageView() = aCore.CreateImageView( aOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT );
        void* mapped     = nullptr;
        if ( vmaMapMemory( allocator, aOut.Allocation(), &mapped ) != VK_SUCCESS || mapped == nullptr ) {
            throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to map fallback image" );
        }
        if ( aFormat == VK_FORMAT_R8_UNORM ) {
            *static_cast< uint8_t* >( mapped ) = 255u;
        }
        else {
            auto* rg                 = static_cast< uint8_t* >( mapped );
            rg[ 0 ]                  = 255u;
            rg[ 1 ]                  = 255u;
        }
        vmaUnmapMemory( allocator, aOut.Allocation() );
    };

    uploadIdentity( VK_FORMAT_R8_UNORM, state.myFallbackAo );
    uploadIdentity( kSoftFormat, state.myFallbackContact );
}

void CreateSoftImage( Vk_Core& aCore, Gfx_Texture& aTexture ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    aCore.CreateImage( extent, kSoftFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
    aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), kSoftFormat, VK_IMAGE_ASPECT_COLOR_BIT );
}

void CreateSoftImages( Vk_Core& aCore ) {
    CreateSoftImage( aCore, aCore.myShadowAoSoftState.mySoftPing );
    CreateSoftImage( aCore, aCore.myShadowAoSoftState.mySoftPong );

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    UtilLogger::Info( "SHADOW-AO-SOFT", "contact soft targets: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=RG8_UNORM" );
}

void UpdatePackDescriptorSet( Vk_Core& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aRawAoView ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo worldPosInfo{};
    worldPosInfo.sampler     = state.myGBufferSampler;
    worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
    worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo rawAoInfo{};
    rawAoInfo.imageView   = aRawAoView;
    rawAoInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo shadowInfo{};
    shadowInfo.sampler     = aCore.myShadowMapState.myCompareSampler;
    shadowInfo.imageView   = aCore.myShadowMapState.myDepth.ImageView();
    shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightingGlobalsInfo{};
    lightingGlobalsInfo.buffer = aCore.myLightingGlobalsBuffer.myBuffer;
    lightingGlobalsInfo.offset = aCore.PadUniformBufferSize( sizeof( GpuLightingGlobals ) ) * aFrameIndex;
    lightingGlobalsInfo.range  = sizeof( GpuLightingGlobals );

    VkDescriptorImageInfo softOutInfo{};
    softOutInfo.imageView   = state.mySoftPing.ImageView();
    softOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 6 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &rawAoInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightingGlobalsInfo, 4, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &softOutInfo, 5, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBlurDescriptorSet( Vk_Core& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aSrcView, VkImageView aDstView ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    (void)aFrameIndex;

    VkDescriptorImageInfo srcInfo{};
    srcInfo.imageView   = aSrcView;
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = aDstView;
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array< VkWriteDescriptorSet, 3 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 2, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateAllDescriptorSets( Vk_Core& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdatePackDescriptorSet( aCore, i, state.myPackDescriptorSets[ i ], Vk_AoPass::GetRawAoImageView( aCore ) );
        UpdatePackDescriptorSet( aCore, i, state.myPackNoAoDescriptorSets[ i ], state.myFallbackAo.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurHorizDescriptorSets[ i ], state.mySoftPing.ImageView(), state.mySoftPong.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurVertDescriptorSets[ i ], state.mySoftPong.ImageView(), state.mySoftPing.ImageView() );
    }
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
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = aOutLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if ( vkCreateComputePipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, computeModule, nullptr );
    return pipeline;
}

void CreatePipelines( Vk_Core& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;

    const std::array< VkDescriptorSetLayoutBinding, 6 > packBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5 ),
    };

    VkDescriptorSetLayoutCreateInfo packLayoutInfo{};
    packLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    packLayoutInfo.bindingCount = static_cast< uint32_t >( packBindings.size() );
    packLayoutInfo.pBindings    = packBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &packLayoutInfo, nullptr, &state.myPackSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create pack descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 3 > blurBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
    };

    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast< uint32_t >( blurBindings.size() );
    blurLayoutInfo.pBindings    = blurBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &blurLayoutInfo, nullptr, &state.myBlurSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create blur descriptor set layout" );
    }

    VkPushConstantRange packPushRange{};
    packPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    packPushRange.offset     = 0;
    packPushRange.size       = sizeof( ShadowAoPackPushConstants );

    const std::string packSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kPackShaderPath );
    state.myPackPipeline          = CreateComputePipeline( aCore, packSpvPath, state.myPackSetLayout, state.myPackPipelineLayout, packPushRange );

    VkPushConstantRange blurPushRange{};
    blurPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    blurPushRange.offset     = 0;
    blurPushRange.size       = sizeof( ShadowAoBlurPushConstants );

    const std::string blurSpvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kBlurShaderPath );
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
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create G-buffer sampler" );
    }

    std::array< VkDescriptorPoolSize, 3 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 10 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 10 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 2 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT * 5;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create descriptor pool" );
    }

    const VkDevice              device             = aCore.myDeviceCtx.myDevice;
    const VkPipeline            packPipeline       = state.myPackPipeline;
    const VkPipeline            blurPipeline       = state.myBlurPipeline;
    const VkPipelineLayout      packPipelineLayout = state.myPackPipelineLayout;
    const VkPipelineLayout      blurPipelineLayout = state.myBlurPipelineLayout;
    const VkDescriptorSetLayout packSetLayout      = state.myPackSetLayout;
    const VkDescriptorSetLayout blurSetLayout      = state.myBlurSetLayout;
    const VkDescriptorPool      descriptorPool     = state.myDescriptorPool;
    const VkSampler             gbufferSampler     = state.myGBufferSampler;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction(
        [ device, packPipeline, blurPipeline, packPipelineLayout, blurPipelineLayout, packSetLayout, blurSetLayout, descriptorPool, gbufferSampler ]() {
            if ( packPipeline != VK_NULL_HANDLE ) {
                vkDestroyPipeline( device, packPipeline, nullptr );
            }
            if ( blurPipeline != VK_NULL_HANDLE ) {
                vkDestroyPipeline( device, blurPipeline, nullptr );
            }
            if ( packPipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, packPipelineLayout, nullptr );
            }
            if ( blurPipelineLayout != VK_NULL_HANDLE ) {
                vkDestroyPipelineLayout( device, blurPipelineLayout, nullptr );
            }
            if ( packSetLayout != VK_NULL_HANDLE ) {
                vkDestroyDescriptorSetLayout( device, packSetLayout, nullptr );
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

    UtilLogger::Info( "PIPELINE", "Shadow/AO contact soft compute pipelines created." );
}

void AllocateDescriptorSets( Vk_Core& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;

        allocInfo.pSetLayouts = &state.myPackSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myPackDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: pack descriptor set alloc" );
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myPackNoAoDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: pack (no SSAO) descriptor set alloc" );

        allocInfo.pSetLayouts = &state.myBlurSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myBlurHorizDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: blur H descriptor set alloc" );
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myBlurVertDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: blur V descriptor set alloc" );
    }
    UpdateAllDescriptorSets( aCore );
}

void CmdBarrierGBufferForPackRead( Vk_Core& aCore, VkCommandBuffer aCommandBuffer ) {
    std::array< VkImageMemoryBarrier, 1 > barriers = {
        ColorImageBarrier( aCore.myGBufferState.myWorldPosition.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT ),
    };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                          static_cast< uint32_t >( barriers.size() ), barriers.data() );
}

void CmdBarrierSoftForComputeWrite( VkCommandBuffer aCommandBuffer, VkImage aImage, VkImageLayout& aInOutLayout ) {
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

void CmdDispatchBlur( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, VkDescriptorSet aSet, float aRadius, float aDepthSigma, uint32_t aAxisX, uint32_t aAxisY,
                      uint32_t aWidth, uint32_t aHeight, const char* aDebugLabel ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBlurPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBlurPipelineLayout, 0, 1, &aSet, 0, nullptr );

    ShadowAoBlurPushConstants push{};
    push.axisX      = aAxisX;
    push.axisY      = aAxisY;
    push.radius     = aRadius;
    push.depthSigma = aDepthSigma;
    vkCmdPushConstants( aCommandBuffer, state.myBlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, aDebugLabel );
    }
    vkCmdDispatch( aCommandBuffer, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }
}

}  // namespace

namespace Vk_ShadowAoSoftPass {

void Destroy( Vk_Core& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroySoftImages( aCore );
    DestroyFallbackImages( aCore );
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        aCore.myShadowAoSoftState.myPackDescriptorSets[ i ]         = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myPackNoAoDescriptorSets[ i ]   = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myBlurHorizDescriptorSets[ i ]    = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myBlurVertDescriptorSets[ i ]     = VK_NULL_HANDLE;
    }
    aCore.myShadowAoSoftState.myInitialized = false;
}

void RecreateForExtent( Vk_Core& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroySoftImages( aCore );
    CreateSoftImages( aCore );
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Core& aCore ) {
    if ( aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_ShadowAoSoftPass::Init." );
    CreatePipelines( aCore );
    CreateFallbackImages( aCore );
    CreateSoftImages( aCore );
    AllocateDescriptorSets( aCore );
    aCore.myShadowAoSoftState.myInitialized = true;
}

void RecordCompute( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, bool aAoPassRan ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
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
    CmdBarrierGBufferForPackRead( aCore, aCommandBuffer );

    const bool useAoRaw = aAoPassRan && aoSettings.myEnabled;
    const VkDescriptorSet packSet =
        useAoRaw ? state.myPackDescriptorSets[ aFrameIndex ] : state.myPackNoAoDescriptorSets[ aFrameIndex ];

    CmdBarrierSoftForComputeWrite( aCommandBuffer, state.mySoftPing.Image(), sSoftPingLayout );
    CmdBarrierSoftForComputeWrite( aCommandBuffer, state.mySoftPong.Image(), sSoftPongLayout );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myPackPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myPackPipelineLayout, 0, 1, &packSet, 0, nullptr );

    ShadowAoPackPushConstants packPush{};
    packPush.params.x = useAoRaw ? 1.0f : 0.0f;
    vkCmdPushConstants( aCommandBuffer, state.myPackPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( packPush ), &packPush );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=ShadowAoPack" );
    }
    vkCmdDispatch( aCommandBuffer, ( width + 7 ) / 8, ( height + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    VkImageMemoryBarrier packedBarrier =
        ColorImageBarrier( state.mySoftPing.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &packedBarrier );

    const float blurRadius   = aoSettings.myContactSoftBlurRadius;
    const float depthSigma   = aoSettings.myContactSoftDepthSigma;
    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurHorizDescriptorSets[ aFrameIndex ], blurRadius, depthSigma, 1, 0, width, height, "Pass=ShadowAoBlurH" );

    VkImageMemoryBarrier pongWritten =
        ColorImageBarrier( state.mySoftPong.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &pongWritten );

    CmdDispatchBlur( aCore, aCommandBuffer, state.myBlurVertDescriptorSets[ aFrameIndex ], blurRadius, depthSigma, 0, 1, width, height, "Pass=ShadowAoBlurV" );

    VkImageMemoryBarrier softForDeferred =
        ColorImageBarrier( state.mySoftPing.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &softForDeferred );
    sSoftPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

VkImageView GetDeferredContactMapView( const Vk_Core& aCore ) {
    const Gfx_AoSettings&       aoSettings = aCore.myAoSettings;
    const Vk_ShadowAoSoftState& softState  = aCore.myShadowAoSoftState;

    if ( aoSettings.myContactSoftEnabled && softState.myInitialized ) {
        return softState.mySoftPing.ImageView();
    }
    if ( aoSettings.myEnabled && aCore.myAoState.myInitialized ) {
        return Vk_AoPass::GetRawAoImageView( aCore );
    }
    if ( softState.myInitialized ) {
        return softState.myFallbackContact.ImageView();
    }
    return VK_NULL_HANDLE;
}

}  // namespace Vk_ShadowAoSoftPass

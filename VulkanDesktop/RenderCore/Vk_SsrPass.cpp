// Module: Vk_SsrPass — Hi-Z screen-space reflections for hybrid deferred (specular-ibl-stack Phase B).
#include "Vk_SsrPass.h"

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_DepthPyramidPass.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace {

constexpr char     kSsrShaderPath[] = "VulkanDesktop/Shader_Generated/SsrTrace.spv";
constexpr VkFormat kSsrFormat       = VK_FORMAT_R16G16B16A16_SFLOAT;

VkImageLayout sSsrLayout = VK_IMAGE_LAYOUT_UNDEFINED;

struct SsrPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::vec4 params;
    alignas( 16 ) glm::uvec4 trace;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( SsrPushConstants ) == 176, "SsrPushConstants must match SsrTrace.comp push block" );

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

void DestroySsrImage( Vk_Renderer& aCore ) {
    const VkDevice     device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator allocator = aCore.myRhi.myDeviceCtx.myAllocator;
    Gfx_Texture&       texture   = aCore.mySsrState.mySsrOutput;

    if ( texture.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( device, texture.ImageView(), nullptr );
        texture.ImageView() = VK_NULL_HANDLE;
    }
    if ( texture.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, texture.Image(), texture.Allocation() );
        texture.AllocImage() = {};
    }
    sSsrLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void CreateSsrImage( Vk_Renderer& aCore ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    Gfx_Texture& texture = aCore.mySsrState.mySsrOutput;
    aCore.CreateImage( extent, kSsrFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, texture.AllocImage() );
    texture.ImageView() = aCore.CreateImageView( texture.Image(), kSsrFormat, VK_IMAGE_ASPECT_COLOR_BIT );

    UtilLogger::Info( "SSR", "target: " + std::to_string( extent.width ) + "x" + std::to_string( extent.height ) + " format=RGBA16F" );
}

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
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

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.sampler     = state.myGBufferSampler;
    albedoInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo hiZInfo{};
    hiZInfo.sampler     = aCore.myDepthPyramidState.myPyramidSampler;
    hiZInfo.imageView   = aCore.myDepthPyramidState.myPyramid.ImageView();
    hiZInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo ssrOutInfo{};
    ssrOutInfo.imageView   = state.mySsrOutput.ImageView();
    ssrOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 6 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &hiZInfo, 4, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &ssrOutInfo, 5, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void CreatePipeline( Vk_Renderer& aCore ) {
    Vk_SsrState& state = aCore.mySsrState;

    const std::array< VkDescriptorSetLayoutBinding, 6 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 4 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myDescriptorSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsrPass: failed to create descriptor set layout" );
    }

    VkPushConstantRange        pushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( SsrPushConstants ) };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.myDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsrPass: failed to create pipeline layout" );
    }

    const std::string spvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kSsrShaderPath );
    VkShaderModule    module  = aCore.CreateShaderModule( spvPath );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, module, "main" );
    pipelineInfo.layout = state.myPipelineLayout;
    if ( vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &state.myComputePipeline )
         != VK_SUCCESS ) {
        vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, module, nullptr );
        throw std::runtime_error( "Vk_SsrPass: failed to create compute pipeline" );
    }
    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, module, nullptr );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsrPass: failed to create G-buffer sampler" );
    }

    std::array< VkDescriptorPoolSize, 2 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 5 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_SsrPass: failed to create descriptor pool" );
    }

    const VkDevice              device              = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipeline            computePipeline     = state.myComputePipeline;
    const VkPipelineLayout      pipelineLayout      = state.myPipelineLayout;
    const VkDescriptorSetLayout descriptorSetLayout = state.myDescriptorSetLayout;
    const VkDescriptorPool      descriptorPool      = state.myDescriptorPool;
    const VkSampler             gbufferSampler      = state.myGBufferSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, computePipeline, pipelineLayout, descriptorSetLayout, descriptorPool, gbufferSampler ]() {
        if ( computePipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( device, computePipeline, nullptr );
        }
        if ( pipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        }
        if ( descriptorSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, descriptorSetLayout, nullptr );
        }
        if ( descriptorPool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        }
        if ( gbufferSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, gbufferSampler, nullptr );
        }
    } );

    UtilLogger::Info( "PIPELINE", "SSR compute pipeline created." );
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
        UpdateDescriptorSet( aCore, i );
    }
}

}  // namespace

namespace Vk_SsrPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroySsrImage( aCore );
    aCore.mySsrState.myDescriptorSets = {};
    aCore.mySsrState.myInitialized    = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroySsrImage( aCore );
    CreateSsrImage( aCore );
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateDescriptorSet( aCore, i );
    }
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.mySsrState.myInitialized ) {
        return;
    }
    if ( !aCore.myGBufferState.myInitialized || !aCore.myDepthPyramidState.myInitialized ) {
        throw std::runtime_error( "Vk_SsrPass::Init requires GBuffer and DepthPyramid" );
    }
    UtilLogger::Info( "FG", "Vk_SsrPass::Init." );
    CreatePipeline( aCore );
    CreateSsrImage( aCore );
    AllocateDescriptorSets( aCore );
    aCore.mySsrState.myInitialized = true;
}

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_SsrState& state = aCore.mySsrState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || state.myDescriptorSets[ aFrameIndex ] == VK_NULL_HANDLE ) {
        return;
    }

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    const VkImageLayout        oldLayout = sSsrLayout;
    VkImageMemoryBarrier       toGeneral = ColorImageBarrier( state.mySsrOutput.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL,
                                                        oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT );
    const VkPipelineStageFlags srcStage  = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral );
    sSsrLayout = VK_IMAGE_LAYOUT_GENERAL;

    SsrPushConstants push{};
    push.view   = aCore.myCamera.myView;
    push.proj   = aCore.myCamera.myProj;
    push.params = glm::vec4( aCore.myLightingSettings.mySsrMaxDistance, aCore.myLightingSettings.mySsrMaxRoughness, aCore.myLightingSettings.mySsrEnabled ? 1.0f : 0.0f,
                             aCore.myLightingSettings.mySsrThickness );
    const uint32_t hiZMaxMip = Vk_DepthPyramidPass::GetMipLevelCount( aCore ) > 0 ? Vk_DepthPyramidPass::GetMipLevelCount( aCore ) - 1 : 0u;
    push.trace               = glm::uvec4( aCore.myLightingSettings.mySsrMaxSteps, hiZMaxMip, 0u, 0u );
    push.screenSize          = glm::vec2( static_cast< float >( extent.width ), static_cast< float >( extent.height ) );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=SSR" );
    }
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myComputePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myPipelineLayout, 0, 1, &state.myDescriptorSets[ aFrameIndex ], 0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
    vkCmdDispatch( aCommandBuffer, ( extent.width + 7 ) / 8, ( extent.height + 7 ) / 8, 1 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    VkImageMemoryBarrier toRead = ColorImageBarrier( state.mySsrOutput.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
                                                     VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead );
    sSsrLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

VkImageView GetOutputImageView( const Vk_Renderer& aCore ) {
    return aCore.mySsrState.mySsrOutput.ImageView();
}

}  // namespace Vk_SsrPass

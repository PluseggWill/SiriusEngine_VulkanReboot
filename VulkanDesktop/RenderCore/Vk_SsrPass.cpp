// Module: Vk_SsrPass — Hi-Z screen-space reflections (specular-ibl-stack Phase B + temporal lit HDR).
#include "Vk_SsrPass.h"

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_DepthPyramidPass.h"
#include "Vk_Initializer.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"

#include <glm/glm.hpp>

#include <array>
#include <stdexcept>
#include <string>

namespace Vk_SsrPassDetail {
VkImageLayout                  gSsrLayout = VK_IMAGE_LAYOUT_UNDEFINED;
std::array< VkImageLayout, 2 > gHistoryLayouts{ VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };
}  // namespace Vk_SsrPassDetail

namespace {

constexpr char     kSsrShaderPath[] = "VulkanDesktop/Shader_Generated/SsrTrace.spv";
constexpr VkFormat kSsrFormat       = VK_FORMAT_R16G16B16A16_SFLOAT;

// Push size must stay aligned with Gfx_SsrPass::TracePush / SsrTrace.comp (used for pipeline layout).
struct SsrPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 prevViewProj;
    alignas( 16 ) glm::vec4 params;
    alignas( 16 ) glm::uvec4 trace;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 4 ) float historyValid;
    alignas( 4 ) float depthRejectSigma;
};

static_assert( sizeof( SsrPushConstants ) == 240, "SsrPushConstants must match SsrTrace.comp push block" );

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
    Vk_SsrState& state = aCore.mySsrState;

    const std::array< VkDescriptorSetLayoutBinding, 7 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 4 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 5 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 6 ),
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
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 7 },
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
    aCore.mySsrState.myDescriptorSets    = {};
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    aCore.mySsrState.myInitialized       = false;
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
    if ( !aCore.myGBufferState.myInitialized || !aCore.myDepthPyramidState.myInitialized || !Vk_PostProcessPass::HasHybridResolve( aCore ) ) {
        throw std::runtime_error( "Vk_SsrPass::Init requires GBuffer, DepthPyramid, and PostProcess hybrid resolve" );
    }
    UtilLogger::Info( "FG", "Vk_SsrPass::Init." );
    CreatePipeline( aCore );
    CreateSsrImage( aCore );
    CreateHistoryImages( aCore );
    aCore.mySsrState.myHistoryReady      = false;
    aCore.mySsrState.myHistoryWriteIndex = 0u;
    AllocateDescriptorSets( aCore );
    aCore.mySsrState.myInitialized = true;
}

// RecordCompute / RecordHistoryUpdate live in Vk_SsrPass_Record.cpp (Gfx_SsrPass via Rhi).

VkImageView GetOutputImageView( const Vk_Renderer& aCore ) {
    return aCore.mySsrState.mySsrOutput.ImageView();
}

}  // namespace Vk_SsrPass

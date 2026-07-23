// Module: Vk_ShadowAoSoftPass — pack raw AO + screen-space sun shadow into RG8, bilateral blur.
// Runs after Vk_AoPass; deferred reads soft ping via GetDeferredContactMapView().
#include "Vk_ShadowAoSoftPass.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_ShadowAoSoftPass.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_AoPass.h"
#include "Vk_FrameCmd.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_ShadowMapPass.h"

#include <vma/vk_mem_alloc.h>

#include <glm/vec4.hpp>

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr char     kPackShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoPack.spv";
constexpr char     kBlurShaderPath[] = "VulkanDesktop/Shader_Generated/ShadowAoBlur.spv";
constexpr VkFormat kSoftFormat       = VK_FORMAT_R8G8_UNORM;

// Mirrors Gfx_ShadowAoSoftPass push blocks — only sizes are needed here for pipeline layout creation.
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

}  // namespace

namespace Vk_ShadowAoSoftPassDetail {
VkImageLayout gSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}  // namespace Vk_ShadowAoSoftPassDetail

namespace {

void DestroySoftTexture( Vk_Renderer& aCore, Vk_TextureResource& aTexture ) {
    const VkDevice     device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator allocator = aCore.myRhi.myDeviceCtx.myAllocator;
    if ( aTexture.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( device, aTexture.ImageView(), nullptr );
        aTexture.ImageView() = VK_NULL_HANDLE;
    }
    if ( aTexture.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, aTexture.Image(), aTexture.Allocation() );
        aTexture.AllocImage() = {};
    }
}

void DestroySoftImages( Vk_Renderer& aCore ) {
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.mySoftPing );
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.mySoftPong );
    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void DestroyFallbackImages( Vk_Renderer& aCore ) {
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.myFallbackAo );
    DestroySoftTexture( aCore, aCore.myShadowAoSoftState.myFallbackContact );
}

void CreateFallbackImages( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState&     state    = aCore.myShadowAoSoftState;
    const Vk_ResourceContext& resource = aCore.GetResourceContext();
    const VkExtent2D          one{ 1, 1 };

    auto uploadIdentity1x1 = [ & ]( VkFormat aFormat, const uint8_t* aBytes, size_t aByteCount, Vk_TextureResource& aOut ) {
        aCore.CreateImage( one, aFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                           VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, aOut.AllocImage() );
        aOut.ImageView() = aCore.CreateImageView( aOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT );

        Vk_AllocatedBuffer staging{};
        resource.CreateBuffer( aByteCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, staging, true );
        void* mapped = nullptr;
        if ( vmaMapMemory( aCore.myRhi.myDeviceCtx.myAllocator, staging.myAllocation, &mapped ) != VK_SUCCESS || mapped == nullptr ) {
            throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to map fallback staging buffer" );
        }
        std::memcpy( mapped, aBytes, aByteCount );
        vmaUnmapMemory( aCore.myRhi.myDeviceCtx.myAllocator, staging.myAllocation );

        resource.TransitionImageLayout( aOut.Image(), aFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1 );
        resource.CopyBufferToImage( staging.myBuffer, aOut.Image(), 1, 1 );
        resource.TransitionImageLayout( aOut.Image(), aFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
        resource.DestroyStagingBuffer( staging );
    };

    const uint8_t aoWhite[]      = { 255u };
    const uint8_t contactWhite[] = { 255u, 255u };
    uploadIdentity1x1( VK_FORMAT_R8_UNORM, aoWhite, sizeof( aoWhite ), state.myFallbackAo );
    uploadIdentity1x1( kSoftFormat, contactWhite, sizeof( contactWhite ), state.myFallbackContact );
}

void CreateSoftImage( Vk_Renderer& aCore, Vk_TextureResource& aTexture ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }

    aCore.CreateImage( extent, kSoftFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
    aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), kSoftFormat, VK_IMAGE_ASPECT_COLOR_BIT );
}

void CreateSoftImages( Vk_Renderer& aCore ) {
    CreateSoftImage( aCore, aCore.myShadowAoSoftState.mySoftPing );
    CreateSoftImage( aCore, aCore.myShadowAoSoftState.mySoftPong );
    // Deferred may bind SoftPing as SHADER_READ_ONLY before the first Soft dispatch (or if Soft is skipped).
    aCore.TransitionImageLayout( aCore.myShadowAoSoftState.mySoftPing.Image(), kSoftFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    aCore.TransitionImageLayout( aCore.myShadowAoSoftState.mySoftPong.Image(), kSoftFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    UtilLogger::Info( "SHADOW-AO-SOFT", "contact soft targets: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=RG8_UNORM" );
}

void UpdatePackDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aRawAoView ) {
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
    rawAoInfo.sampler     = state.myGBufferSampler;
    rawAoInfo.imageView   = aRawAoView;
    rawAoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo shadowInfo{};
    shadowInfo.sampler     = aCore.myShadowMapState.myCompareSampler;
    shadowInfo.imageView   = aCore.myShadowMapState.myDepth.ImageView();
    shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightingGlobalsInfo{};
    lightingGlobalsInfo.buffer = aCore.myLightingGlobalsBuffer.myBuffer;
    lightingGlobalsInfo.offset = aCore.PadUniformBufferSize( sizeof( Gpu_LightingGlobals ) ) * aFrameIndex;
    lightingGlobalsInfo.range  = sizeof( Gpu_LightingGlobals );

    VkDescriptorImageInfo softOutInfo{};
    softOutInfo.imageView   = state.mySoftPing.ImageView();
    softOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 6 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &rawAoInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightingGlobalsInfo, 4, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &softOutInfo, 5, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBlurDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aSrcView, VkImageView aDstView ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    ( void )aFrameIndex;

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
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateAllDescriptorSets( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdatePackDescriptorSet( aCore, i, state.myPackDescriptorSets[ i ], Vk_AoPass::GetRawAoImageView( aCore ) );
        UpdatePackDescriptorSet( aCore, i, state.myPackNoAoDescriptorSets[ i ], state.myFallbackAo.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurHorizDescriptorSets[ i ], state.mySoftPing.ImageView(), state.mySoftPong.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurVertDescriptorSets[ i ], state.mySoftPong.ImageView(), state.mySoftPing.ImageView() );
    }
}

VkPipeline CreateComputePipeline( Vk_Renderer& aCore, const std::string& aSpvPath, VkDescriptorSetLayout aSetLayout, VkPipelineLayout& aOutLayout,
                                  VkPushConstantRange aPushRange ) {
    VkShaderModule computeModule = aCore.CreateShaderModule( aSpvPath );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &aSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &aPushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &aOutLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = aOutLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if ( vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, computeModule, nullptr );
    return pipeline;
}

void CreatePipelines( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;

    const std::array< VkDescriptorSetLayoutBinding, 6 > packBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5 ),
    };

    VkDescriptorSetLayoutCreateInfo packLayoutInfo{};
    packLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    packLayoutInfo.bindingCount = static_cast< uint32_t >( packBindings.size() );
    packLayoutInfo.pBindings    = packBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &packLayoutInfo, nullptr, &state.myPackSetLayout ) != VK_SUCCESS ) {
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
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &blurLayoutInfo, nullptr, &state.myBlurSetLayout ) != VK_SUCCESS ) {
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
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
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
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowAoSoftPass: failed to create descriptor pool" );
    }

    const VkDevice              device             = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipeline            packPipeline       = state.myPackPipeline;
    const VkPipeline            blurPipeline       = state.myBlurPipeline;
    const VkPipelineLayout      packPipelineLayout = state.myPackPipelineLayout;
    const VkPipelineLayout      blurPipelineLayout = state.myBlurPipelineLayout;
    const VkDescriptorSetLayout packSetLayout      = state.myPackSetLayout;
    const VkDescriptorSetLayout blurSetLayout      = state.myBlurSetLayout;
    const VkDescriptorPool      descriptorPool     = state.myDescriptorPool;
    const VkSampler             gbufferSampler     = state.myGBufferSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction(
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

void AllocateDescriptorSets( Vk_Renderer& aCore ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;

        allocInfo.pSetLayouts = &state.myPackSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myPackDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: pack descriptor set alloc" );
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myPackNoAoDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: pack (no SSAO) descriptor set alloc" );

        allocInfo.pSetLayouts = &state.myBlurSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBlurHorizDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: blur H descriptor set alloc" );
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBlurVertDescriptorSets[ i ] ),
                                          "Vk_ShadowAoSoftPass: blur V descriptor set alloc" );
    }
    UpdateAllDescriptorSets( aCore );
}

}  // namespace

namespace Vk_ShadowAoSoftPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroySoftImages( aCore );
    DestroyFallbackImages( aCore );
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        aCore.myShadowAoSoftState.myPackDescriptorSets[ i ]      = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myPackNoAoDescriptorSets[ i ]  = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myBlurHorizDescriptorSets[ i ] = VK_NULL_HANDLE;
        aCore.myShadowAoSoftState.myBlurVertDescriptorSets[ i ]  = VK_NULL_HANDLE;
    }
    aCore.myShadowAoSoftState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myShadowAoSoftState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroySoftImages( aCore );
    CreateSoftImages( aCore );
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Renderer& aCore ) {
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

// RecordCompute via Gfx_ShadowAoSoftPass::Record (Rhi + Vk_FrameCmd).

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, bool aAoPassRan ) {
    Vk_ShadowAoSoftState& state = aCore.myShadowAoSoftState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
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

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    const bool            useAoRaw  = aAoPassRan && aoSettings.myEnabled;
    const VkDescriptorSet packSetVk = useAoRaw ? state.myPackDescriptorSets[ aFrameIndex ] : state.myPackNoAoDescriptorSets[ aFrameIndex ];

    Gfx_ShadowAoSoftPass::GpuResources gpu{};
    gpu.myPackPipeline    = RhiVulkan::PipelineAdopt( state.myPackPipeline );
    gpu.myBlurPipeline    = RhiVulkan::PipelineAdopt( state.myBlurPipeline );
    gpu.myPackLayout      = RhiVulkan::PipelineLayoutAdopt( state.myPackPipelineLayout );
    gpu.myBlurLayout      = RhiVulkan::PipelineLayoutAdopt( state.myBlurPipelineLayout );
    gpu.myPackSet         = RhiVulkan::DescriptorSetAdopt( packSetVk );
    gpu.myBlurHorizSet    = RhiVulkan::DescriptorSetAdopt( state.myBlurHorizDescriptorSets[ aFrameIndex ] );
    gpu.myBlurVertSet     = RhiVulkan::DescriptorSetAdopt( state.myBlurVertDescriptorSets[ aFrameIndex ] );
    gpu.mySoftPing        = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySoftPing.Image(), state.mySoftPing.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
    gpu.mySoftPong        = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, state.mySoftPong.Image(), state.mySoftPong.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
    gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                     VK_FORMAT_R16G16B16A16_SFLOAT, 1 );

    Rhi_ImageLayout pingLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_ShadowAoSoftPassDetail::gSoftPingLayout );
    Rhi_ImageLayout pongLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_ShadowAoSoftPassDetail::gSoftPongLayout );
    Rhi_ImageLayout aoRawLayout{};
    if ( useAoRaw ) {
        gpu.myAoRaw = RhiVulkan::TextureAdopt( aCore.myGfxRhiDevice, aCore.myAoState.myAoRaw.Image(), aCore.myAoState.myAoRaw.ImageView(), VK_FORMAT_R8_UNORM, 1 );
        aoRawLayout = Vk_FrameCmd::ImageLayoutFromVk( Vk_AoPass::GetRawAoLayout() );
    }

    Gfx_ShadowAoSoftPass::RecordInput input{};
    input.myWidth       = width;
    input.myHeight      = height;
    input.myDebugLabels = aCore.AreCommandDebugLabelsEnabled();
    input.myUseAoRaw    = useAoRaw;
    input.myBlurRadius  = aoSettings.myContactSoftBlurRadius;
    input.myDepthSigma  = aoSettings.myContactSoftDepthSigma;
    input.myPingLayout  = &pingLayout;
    input.myPongLayout  = &pongLayout;
    input.myAoRawLayout = useAoRaw ? &aoRawLayout : nullptr;

    Gfx_ShadowAoSoftPass::Record( frameCmd.Get(), gpu, input );

    Vk_ShadowAoSoftPassDetail::gSoftPingLayout = Vk_FrameCmd::ImageLayoutToVk( pingLayout );
    Vk_ShadowAoSoftPassDetail::gSoftPongLayout = Vk_FrameCmd::ImageLayoutToVk( pongLayout );
    if ( useAoRaw ) {
        Vk_AoPass::NoteRawAoLayout( Vk_FrameCmd::ImageLayoutToVk( aoRawLayout ) );
        Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myAoRaw );
    }

    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySoftPing );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.mySoftPong );
    Rhi::DeviceDestroyTexture( aCore.myGfxRhiDevice, gpu.myGBufferWorldPos );
}

VkImageView GetDeferredContactMapView( const Vk_Renderer& aCore ) {
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

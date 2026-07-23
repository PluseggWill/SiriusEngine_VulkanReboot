// Module: Vk_PostProcessPass — HDR hybrid resolve target + tonemap/bloom to swapchain.
#include "Vk_PostProcessPass.h"

#include "Vk_PostProcessPassInternal.h"

#include "../Gfx/Gfx_PostSettings.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include "../Rhi/Rhi_Device.h"

#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char     kTonemapVertSpv[]    = "VulkanDesktop/Shader_Generated/TonemapVert.spv";
constexpr char     kTonemapFragSpv[]    = "VulkanDesktop/Shader_Generated/TonemapFrag.spv";
constexpr char     kBloomThresholdSpv[] = "VulkanDesktop/Shader_Generated/BloomThreshold.spv";
constexpr char     kBloomBlurSpv[]      = "VulkanDesktop/Shader_Generated/BloomBlur.spv";
constexpr char     kTaaResolveSpv[]     = "VulkanDesktop/Shader_Generated/TaaResolve.spv";
constexpr VkFormat kBloomFormat         = VK_FORMAT_R16G16B16A16_SFLOAT;

struct TonemapPushConstants {
    float    exposure;
    float    bloomIntensity;
    uint32_t tonemapEnabled;
    uint32_t bloomEnabled;
    uint32_t tonemapMode;
};

static_assert( sizeof( TonemapPushConstants ) == 20, "TonemapPushConstants must match Tonemap.frag push block" );

struct BloomThresholdPushConstants {
    float threshold;
    float pad0;
    float pad1;
    float pad2;
};

static_assert( sizeof( BloomThresholdPushConstants ) == 16, "BloomThresholdPushConstants must match BloomThreshold.comp push block" );

struct BloomBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( BloomBlurPushConstants ) == 8, "BloomBlurPushConstants must match BloomBlur.comp push block" );

struct TaaResolvePushConstants {
    float historyBlend;
    float historyValid;
    float varianceGamma;
    float sharpen;
};
static_assert( sizeof( TaaResolvePushConstants ) == 16, "TaaResolvePushConstants must match TaaResolve.comp push block" );

void DestroyTexture( Vk_Renderer& aCore, Vk_TextureResource& aTexture ) {
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

VkExtent2D BloomExtent( const VkExtent2D& aFullExtent ) {
    return { std::max( 1u, aFullExtent.width / 2 ), std::max( 1u, aFullExtent.height / 2 ) };
}

void CreateSceneColorImage( Vk_Renderer& aCore ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }
    aCore.CreateImage( extent, kPostSceneColorFormat, VK_IMAGE_TILING_OPTIMAL,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, aCore.myPostProcessState.mySceneColor.AllocImage() );
    aCore.myPostProcessState.mySceneColor.ImageView() =
        aCore.CreateImageView( aCore.myPostProcessState.mySceneColor.Image(), kPostSceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT );
}

void CreateTaaImages( Vk_Renderer& aCore ) {
    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    if ( extent.width == 0 || extent.height == 0 ) {
        return;
    }
    // Resolved: compute write + tonemap sample + history copy src.
    aCore.CreateImage( extent, kPostSceneColorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, aCore.myPostProcessState.myTaaResolved.AllocImage() );
    aCore.myPostProcessState.myTaaResolved.ImageView() =
        aCore.CreateImageView( aCore.myPostProcessState.myTaaResolved.Image(), kPostSceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT );

    for ( uint32_t i = 0; i < 2; ++i ) {
        // History: sample + copy dst (and src if ping-pong ever blits history).
        aCore.CreateImage( extent, kPostSceneColorFormat, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, aCore.myPostProcessState.myTaaHistory[ i ].AllocImage() );
        aCore.myPostProcessState.myTaaHistory[ i ].ImageView() =
            aCore.CreateImageView( aCore.myPostProcessState.myTaaHistory[ i ].Image(), kPostSceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT );
    }
    aCore.myPostProcessState.myTaaHistoryWriteIndex = 0u;
}

void CreateBloomImage( Vk_Renderer& aCore, Vk_TextureResource& aTexture, VkExtent2D aExtent ) {
    aCore.CreateImage( aExtent, kBloomFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
    aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), kBloomFormat, VK_IMAGE_ASPECT_COLOR_BIT );
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
    if ( !aCore.myGfxRhiDevice || !aCore.myPostProcessState.myRhiHybridRenderPass ) {
        throw std::runtime_error( "Vk_PostProcessPass: hybrid render pass required before framebuffer" );
    }

    Rhi_Device& rhi = aCore.myGfxRhiDevice;
    Rhi_Texture colorTex =
        RhiVulkan::TextureAdopt( rhi, aCore.myPostProcessState.mySceneColor.Image(), aCore.myPostProcessState.mySceneColor.ImageView(), kPostSceneColorFormat, 1 );
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
    Rhi::DeviceDestroyTexture( rhi, colorTex );
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

VkPipeline BuildTonemapPipeline( Vk_Renderer& aCore, VkRenderPass aRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_PostProcessPass: myGfxRhiDevice required for tonemap PSO" );
    }
    Rhi_Device& rhi = aCore.myGfxRhiDevice;

    const std::vector< char > vertSpirv = LoadSpirvFile( aVertPath );
    const std::vector< char > fragSpirv = LoadSpirvFile( aFragPath );
    Rhi_ShaderModule          vert      = Rhi::DeviceCreateShaderModule( rhi, vertSpirv.data(), vertSpirv.size() );
    Rhi_ShaderModule          frag      = Rhi::DeviceCreateShaderModule( rhi, fragSpirv.data(), fragSpirv.size() );
    if ( !vert || !frag ) {
        Rhi::DeviceDestroyShaderModule( rhi, vert );
        Rhi::DeviceDestroyShaderModule( rhi, frag );
        throw std::runtime_error( "Vk_PostProcessPass: tonemap shader module create failed" );
    }

    uint32_t sampleCount = 1;
    switch ( aCore.mySwapchainCtx.myMSAASamples ) {
    case VK_SAMPLE_COUNT_2_BIT:
        sampleCount = 2;
        break;
    case VK_SAMPLE_COUNT_4_BIT:
        sampleCount = 4;
        break;
    case VK_SAMPLE_COUNT_8_BIT:
        sampleCount = 8;
        break;
    default:
        sampleCount = 1;
        break;
    }

    Rhi::GraphicsPipelineDesc desc{};
    desc.myVertexShader           = vert;
    desc.myFragmentShader         = frag;
    desc.myLayout                 = RhiVulkan::PipelineLayoutAdopt( aLayout );
    desc.myRenderPass             = RhiVulkan::RenderPassAdopt( aRenderPass );
    desc.myColorAttachmentCount   = 1;
    desc.mySampleCount            = sampleCount;
    desc.myCullMode               = Rhi_CullMode::None;
    desc.myDepthTestEnable        = false;
    desc.myDepthWriteEnable       = false;
    desc.myBlendEnable            = false;
    desc.myDynamicViewportScissor = true;

    if ( aCore.myPostProcessState.myRhiTonemapPipeline ) {
        Rhi::DeviceDestroyPipeline( rhi, aCore.myPostProcessState.myRhiTonemapPipeline );
        aCore.myPostProcessState.myTonemapPipeline = VK_NULL_HANDLE;
    }
    aCore.myPostProcessState.myRhiTonemapPipeline = Rhi::DeviceCreateGraphicsPipeline( rhi, desc );
    Rhi::DeviceDestroyShaderModule( rhi, vert );
    Rhi::DeviceDestroyShaderModule( rhi, frag );
    if ( !aCore.myPostProcessState.myRhiTonemapPipeline ) {
        throw std::runtime_error( "Vk_PostProcessPass: DeviceCreateGraphicsPipeline tonemap failed" );
    }
    aCore.myPostProcessState.myTonemapPipeline = RhiVulkan::PipelineGetVk( rhi, aCore.myPostProcessState.myRhiTonemapPipeline );
    return aCore.myPostProcessState.myTonemapPipeline;
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

VkPipeline BuildComputePipeline( Vk_Renderer& aCore, VkPipelineLayout aLayout, const std::string& aShaderPath ) {
    VkShaderModule module = aCore.CreateShaderModule( aShaderPath );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, module, "main" );
    pipelineInfo.layout = aLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    UtilVulkanResult::ThrowOnFailure(
        vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ),
        "vkCreateComputePipelines PostProcess" );
    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, module, nullptr );
    return pipeline;
}

void UpdateTonemapDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = state.mySceneSampler;
    sceneInfo.imageView   = ( post.myTaaEnabled && state.myTaaResolved.ImageView() != VK_NULL_HANDLE ) ? state.myTaaResolved.ImageView() : state.mySceneColor.ImageView();
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo bloomInfo{};
    bloomInfo.sampler     = state.mySceneSampler;
    bloomInfo.imageView   = state.myBloomPing.ImageView();
    bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTonemapDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTonemapDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &bloomInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateTaaResolveDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = state.mySceneSampler;
    sceneInfo.imageView   = state.mySceneColor.ImageView();
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    const uint32_t        readIndex = 1u - state.myTaaHistoryWriteIndex;
    VkDescriptorImageInfo historyInfo{};
    historyInfo.sampler     = state.mySceneSampler;
    historyInfo.imageView   = state.myTaaHistory[ readIndex ].ImageView();
    historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo mvInfo{};
    mvInfo.sampler     = state.mySceneSampler;
    mvInfo.imageView   = aCore.myGBufferState.myMotionVector.ImageView();
    mvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = state.myTaaResolved.ImageView();
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &historyInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mvInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTaaResolveDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBloomThresholdDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;

    VkDescriptorImageInfo sceneInfo{};
    sceneInfo.sampler     = state.mySceneSampler;
    sceneInfo.imageView   = state.mySceneColor.ImageView();
    sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo bloomOutInfo{};
    bloomOutInfo.imageView   = state.myBloomPing.ImageView();
    bloomOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myBloomThresholdDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myBloomThresholdDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &bloomOutInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBloomBlurDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, bool aHorizontal ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;

    VkDescriptorImageInfo srcInfo{};
    srcInfo.imageView   = aHorizontal ? state.myBloomPing.ImageView() : state.myBloomPong.ImageView();
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = aHorizontal ? state.myBloomPong.ImageView() : state.myBloomPing.ImageView();
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorSet set = aHorizontal ? state.myBloomBlurHorizDescriptorSets[ aFrameIndex ] : state.myBloomBlurVertDescriptorSets[ aFrameIndex ];

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void CreatePipelineResources( Vk_Renderer& aCore ) {
    if ( aCore.mySwapchainCtx.myRenderPass == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_PostProcessPass: swapchain render pass is required" );
    }

    Vk_PostProcessState& state = aCore.myPostProcessState;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.mySceneSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: failed to create sampler" );
    }
    const VkDevice  device  = aCore.myRhi.myDeviceCtx.myDevice;
    const VkSampler sampler = state.mySceneSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, sampler ]() { vkDestroySampler( device, sampler, nullptr ); } );

    const std::array< VkDescriptorSetLayoutBinding, 2 > tonemapBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
    };
    VkDescriptorSetLayoutCreateInfo tonemapLayoutInfo{};
    tonemapLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    tonemapLayoutInfo.bindingCount = static_cast< uint32_t >( tonemapBindings.size() );
    tonemapLayoutInfo.pBindings    = tonemapBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &tonemapLayoutInfo, nullptr, &state.myTonemapSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: tonemap descriptor layout failed" );
    }

    VkPushConstantRange tonemapPush{};
    tonemapPush.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    tonemapPush.offset     = 0;
    tonemapPush.size       = sizeof( TonemapPushConstants );

    VkPipelineLayoutCreateInfo tonemapPipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    tonemapPipelineLayoutInfo.setLayoutCount             = 1;
    tonemapPipelineLayoutInfo.pSetLayouts                = &state.myTonemapSetLayout;
    tonemapPipelineLayoutInfo.pushConstantRangeCount     = 1;
    tonemapPipelineLayoutInfo.pPushConstantRanges        = &tonemapPush;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &tonemapPipelineLayoutInfo, nullptr, &state.myTonemapPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: tonemap pipeline layout failed" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 2 > thresholdBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
    };
    VkDescriptorSetLayoutCreateInfo thresholdLayoutInfo{};
    thresholdLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    thresholdLayoutInfo.bindingCount = static_cast< uint32_t >( thresholdBindings.size() );
    thresholdLayoutInfo.pBindings    = thresholdBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &thresholdLayoutInfo, nullptr, &state.myBloomThresholdSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: bloom threshold descriptor layout failed" );
    }

    VkPushConstantRange thresholdPush{};
    thresholdPush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    thresholdPush.offset     = 0;
    thresholdPush.size       = sizeof( BloomThresholdPushConstants );

    VkPipelineLayoutCreateInfo thresholdPipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    thresholdPipelineLayoutInfo.setLayoutCount             = 1;
    thresholdPipelineLayoutInfo.pSetLayouts                = &state.myBloomThresholdSetLayout;
    thresholdPipelineLayoutInfo.pushConstantRangeCount     = 1;
    thresholdPipelineLayoutInfo.pPushConstantRanges        = &thresholdPush;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &thresholdPipelineLayoutInfo, nullptr, &state.myBloomThresholdPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: bloom threshold pipeline layout failed" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 2 > blurBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
    };
    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast< uint32_t >( blurBindings.size() );
    blurLayoutInfo.pBindings    = blurBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &blurLayoutInfo, nullptr, &state.myBloomBlurSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: bloom blur descriptor layout failed" );
    }

    VkPushConstantRange blurPush{};
    blurPush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    blurPush.offset     = 0;
    blurPush.size       = sizeof( BloomBlurPushConstants );

    VkPipelineLayoutCreateInfo blurPipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    blurPipelineLayoutInfo.setLayoutCount             = 1;
    blurPipelineLayoutInfo.pSetLayouts                = &state.myBloomBlurSetLayout;
    blurPipelineLayoutInfo.pushConstantRangeCount     = 1;
    blurPipelineLayoutInfo.pPushConstantRanges        = &blurPush;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &blurPipelineLayoutInfo, nullptr, &state.myBloomBlurPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: bloom blur pipeline layout failed" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 4 > taaBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
    };
    VkDescriptorSetLayoutCreateInfo taaLayoutInfo{};
    taaLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    taaLayoutInfo.bindingCount = static_cast< uint32_t >( taaBindings.size() );
    taaLayoutInfo.pBindings    = taaBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &taaLayoutInfo, nullptr, &state.myTaaResolveSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: TAA resolve descriptor layout failed" );
    }

    VkPushConstantRange taaPush{};
    taaPush.stageFlags                               = VK_SHADER_STAGE_COMPUTE_BIT;
    taaPush.offset                                   = 0;
    taaPush.size                                     = sizeof( TaaResolvePushConstants );
    VkPipelineLayoutCreateInfo taaPipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    taaPipelineLayoutInfo.setLayoutCount             = 1;
    taaPipelineLayoutInfo.pSetLayouts                = &state.myTaaResolveSetLayout;
    taaPipelineLayoutInfo.pushConstantRangeCount     = 1;
    taaPipelineLayoutInfo.pPushConstantRanges        = &taaPush;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &taaPipelineLayoutInfo, nullptr, &state.myTaaResolvePipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: TAA resolve pipeline layout failed" );
    }

    // Per frame: tonemap(1) + taa(1) + threshold(1) + blurH(1) + blurV(1) storage images — match Vk_AoPass pool sizing.
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT * 6 ) },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT * 6 ) },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( std::size( poolSizes ) );
    poolInfo.pPoolSizes    = poolSizes;
    poolInfo.maxSets       = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT * 5 );
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_PostProcessPass: descriptor pool failed" );
    }

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;

        allocInfo.pSetLayouts = &state.myTonemapSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myTonemapDescriptorSets[ i ] ),
                                          "Vk_PostProcessPass: tonemap descriptor set alloc" );

        allocInfo.pSetLayouts = &state.myTaaResolveSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myTaaResolveDescriptorSets[ i ] ),
                                          "Vk_PostProcessPass: TAA resolve descriptor set alloc" );

        allocInfo.pSetLayouts = &state.myBloomThresholdSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBloomThresholdDescriptorSets[ i ] ),
                                          "Vk_PostProcessPass: bloom threshold descriptor set alloc" );

        allocInfo.pSetLayouts = &state.myBloomBlurSetLayout;
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBloomBlurHorizDescriptorSets[ i ] ),
                                          "Vk_PostProcessPass: bloom blur H descriptor set alloc" );
        UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBloomBlurVertDescriptorSets[ i ] ),
                                          "Vk_PostProcessPass: bloom blur V descriptor set alloc" );
    }

    // Pipelines are built in RebuildResources (Init always calls it; also on resize).
}

void RebuildResources( Vk_Renderer& aCore ) {
    if ( aCore.myRhi.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }

    // Extent-scoped only (RP/FB). Sampler + descriptor pool live for pass lifetime — never flush them here.
    DestroyHybridResolveRhi( aCore );
    aCore.myPostProcessState.myDeletionQueue.flush();
    Vk_PostProcessPassDetail::ResetImageLayouts();

    Vk_PostProcessState& state = aCore.myPostProcessState;
    DestroyTexture( aCore, state.mySceneColor );
    DestroyTexture( aCore, state.myTaaResolved );
    DestroyTexture( aCore, state.myTaaHistory[ 0 ] );
    DestroyTexture( aCore, state.myTaaHistory[ 1 ] );
    DestroyTexture( aCore, state.myBloomPing );
    DestroyTexture( aCore, state.myBloomPong );
    state.myTaaHistoryReady      = false;
    state.myTaaHistoryWriteIndex = 0u;

    CreateSceneColorImage( aCore );
    CreateTaaImages( aCore );
    const VkExtent2D bloomExtent = BloomExtent( aCore.mySwapchainCtx.mySwapChainExtent );
    CreateBloomImage( aCore, aCore.myPostProcessState.myBloomPing, bloomExtent );
    CreateBloomImage( aCore, aCore.myPostProcessState.myBloomPong, bloomExtent );

    CreateHybridRenderPass( aCore );
    CreateHybridFramebuffer( aCore );

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    UtilLogger::Info( "POST", "HDR scene color: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " format=R16G16B16A16_SFLOAT" );

    if ( aCore.myPostProcessState.myRhiTonemapPipeline ) {
        Rhi::DeviceDestroyPipeline( aCore.myGfxRhiDevice, aCore.myPostProcessState.myRhiTonemapPipeline );
        aCore.myPostProcessState.myTonemapPipeline = VK_NULL_HANDLE;
    }
    else if ( aCore.myPostProcessState.myTonemapPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( aCore.myRhi.myDeviceCtx.myDevice, aCore.myPostProcessState.myTonemapPipeline, nullptr );
        aCore.myPostProcessState.myTonemapPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myPostProcessState.myTaaResolvePipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( aCore.myRhi.myDeviceCtx.myDevice, aCore.myPostProcessState.myTaaResolvePipeline, nullptr );
        aCore.myPostProcessState.myTaaResolvePipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myPostProcessState.myBloomThresholdPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( aCore.myRhi.myDeviceCtx.myDevice, aCore.myPostProcessState.myBloomThresholdPipeline, nullptr );
        aCore.myPostProcessState.myBloomThresholdPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myPostProcessState.myBloomBlurPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( aCore.myRhi.myDeviceCtx.myDevice, aCore.myPostProcessState.myBloomBlurPipeline, nullptr );
        aCore.myPostProcessState.myBloomBlurPipeline = VK_NULL_HANDLE;
    }

    // Descriptor sets are allocated in CreatePipelineResources; skip until pool exists (Init order).
    if ( aCore.myPostProcessState.myDescriptorPool != VK_NULL_HANDLE ) {
        for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
            UpdateTaaResolveDescriptorSet( aCore, i );
            UpdateTonemapDescriptorSet( aCore, i );
            UpdateBloomThresholdDescriptorSet( aCore, i );
            UpdateBloomBlurDescriptorSet( aCore, i, true );
            UpdateBloomBlurDescriptorSet( aCore, i, false );
        }
    }

    if ( aCore.myPostProcessState.myTonemapPipelineLayout != VK_NULL_HANDLE ) {
        const std::string tonemapVertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapVertSpv );
        const std::string tonemapFragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapFragSpv );
        aCore.myPostProcessState.myTonemapPipeline =
            BuildTonemapPipeline( aCore, aCore.mySwapchainCtx.myRenderPass, aCore.myPostProcessState.myTonemapPipelineLayout, tonemapVertPath, tonemapFragPath );
    }
    if ( aCore.myPostProcessState.myBloomThresholdPipelineLayout != VK_NULL_HANDLE ) {
        const std::string thresholdPath                   = UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomThresholdSpv );
        aCore.myPostProcessState.myBloomThresholdPipeline = BuildComputePipeline( aCore, aCore.myPostProcessState.myBloomThresholdPipelineLayout, thresholdPath );
    }
    if ( aCore.myPostProcessState.myBloomBlurPipelineLayout != VK_NULL_HANDLE ) {
        const std::string blurPath                   = UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomBlurSpv );
        aCore.myPostProcessState.myBloomBlurPipeline = BuildComputePipeline( aCore, aCore.myPostProcessState.myBloomBlurPipelineLayout, blurPath );
    }
    if ( aCore.myPostProcessState.myTaaResolvePipelineLayout != VK_NULL_HANDLE ) {
        const std::string taaPath                     = UtilLoader::ResolvePath( aCore.EngineConfig(), kTaaResolveSpv );
        aCore.myPostProcessState.myTaaResolvePipeline = BuildComputePipeline( aCore, aCore.myPostProcessState.myTaaResolvePipelineLayout, taaPath );
    }
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

    Vk_PostProcessState& state  = aCore.myPostProcessState;
    const VkDevice       device = aCore.myRhi.myDeviceCtx.myDevice;
    if ( state.myRhiTonemapPipeline ) {
        Rhi::DeviceDestroyPipeline( aCore.myGfxRhiDevice, state.myRhiTonemapPipeline );
        state.myTonemapPipeline = VK_NULL_HANDLE;
    }
    else if ( state.myTonemapPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, state.myTonemapPipeline, nullptr );
        state.myTonemapPipeline = VK_NULL_HANDLE;
    }
    if ( state.myTaaResolvePipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, state.myTaaResolvePipeline, nullptr );
        state.myTaaResolvePipeline = VK_NULL_HANDLE;
    }
    if ( state.myBloomThresholdPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, state.myBloomThresholdPipeline, nullptr );
        state.myBloomThresholdPipeline = VK_NULL_HANDLE;
    }
    if ( state.myBloomBlurPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, state.myBloomBlurPipeline, nullptr );
        state.myBloomBlurPipeline = VK_NULL_HANDLE;
    }
    if ( state.myTonemapPipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, state.myTonemapPipelineLayout, nullptr );
        state.myTonemapPipelineLayout = VK_NULL_HANDLE;
    }
    if ( state.myTaaResolvePipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, state.myTaaResolvePipelineLayout, nullptr );
        state.myTaaResolvePipelineLayout = VK_NULL_HANDLE;
    }
    if ( state.myBloomThresholdPipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, state.myBloomThresholdPipelineLayout, nullptr );
        state.myBloomThresholdPipelineLayout = VK_NULL_HANDLE;
    }
    if ( state.myBloomBlurPipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, state.myBloomBlurPipelineLayout, nullptr );
        state.myBloomBlurPipelineLayout = VK_NULL_HANDLE;
    }
    if ( state.myTonemapSetLayout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( device, state.myTonemapSetLayout, nullptr );
        state.myTonemapSetLayout = VK_NULL_HANDLE;
    }
    if ( state.myTaaResolveSetLayout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( device, state.myTaaResolveSetLayout, nullptr );
        state.myTaaResolveSetLayout = VK_NULL_HANDLE;
    }
    if ( state.myBloomThresholdSetLayout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( device, state.myBloomThresholdSetLayout, nullptr );
        state.myBloomThresholdSetLayout = VK_NULL_HANDLE;
    }
    if ( state.myBloomBlurSetLayout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( device, state.myBloomBlurSetLayout, nullptr );
        state.myBloomBlurSetLayout = VK_NULL_HANDLE;
    }
    if ( state.myDescriptorPool != VK_NULL_HANDLE ) {
        vkDestroyDescriptorPool( device, state.myDescriptorPool, nullptr );
        state.myDescriptorPool = VK_NULL_HANDLE;
    }

    state.myDeletionQueue.flush();
    DestroyHybridResolveRhi( aCore );
    DestroyTexture( aCore, state.mySceneColor );
    DestroyTexture( aCore, state.myTaaResolved );
    DestroyTexture( aCore, state.myTaaHistory[ 0 ] );
    DestroyTexture( aCore, state.myTaaHistory[ 1 ] );
    DestroyTexture( aCore, state.myBloomPing );
    DestroyTexture( aCore, state.myBloomPong );
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

}  // namespace Vk_PostProcessPass

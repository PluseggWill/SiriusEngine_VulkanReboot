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

#include <array>
#include <stdexcept>
#include <string>

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
    VkAttachmentDescription color{};
    color.format         = kPostSceneColorFormat;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format         = aCore.FindDepthFormat();
    depth.samples        = aCore.mySwapchainCtx.myMSAASamples;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array< VkAttachmentDescription, 2 > attachments = { color, depth };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    UtilVulkanResult::ThrowOnFailure( vkCreateRenderPass( aCore.myRhi.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.myPostProcessState.myHybridRenderPass ),
                                      "vkCreateRenderPass PostProcess hybrid" );

    const VkDevice     device     = aCore.myRhi.myDeviceCtx.myDevice;
    const VkRenderPass renderPass = aCore.myPostProcessState.myHybridRenderPass;
    aCore.myPostProcessState.myDeletionQueue.pushFunction( [ device, renderPass ]() { vkDestroyRenderPass( device, renderPass, nullptr ); } );
}

void CreateHybridFramebuffer( Vk_Renderer& aCore ) {
    std::array< VkImageView, 2 > attachments = { aCore.myPostProcessState.mySceneColor.ImageView(), aCore.mySwapchainCtx.myDepthTexture.ImageView() };

    VkFramebufferCreateInfo frameBufferInfo{};
    frameBufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferInfo.renderPass      = aCore.myPostProcessState.myHybridRenderPass;
    frameBufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    frameBufferInfo.pAttachments    = attachments.data();
    frameBufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
    frameBufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
    frameBufferInfo.layers          = 1;

    UtilVulkanResult::ThrowOnFailure( vkCreateFramebuffer( aCore.myRhi.myDeviceCtx.myDevice, &frameBufferInfo, nullptr, &aCore.myPostProcessState.myHybridFramebuffer ),
                                      "vkCreateFramebuffer PostProcess hybrid" );

    const VkDevice      device      = aCore.myRhi.myDeviceCtx.myDevice;
    const VkFramebuffer framebuffer = aCore.myPostProcessState.myHybridFramebuffer;
    aCore.myPostProcessState.myDeletionQueue.pushFunction( [ device, framebuffer ]() { vkDestroyFramebuffer( device, framebuffer, nullptr ); } );
}

VkPipeline BuildTonemapPipeline( Vk_Renderer& aCore, VkRenderPass aRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo               = VkInit::Pipeline_VertexInputStateCreateInfo();
    pipelineBuilder.myInputAssembly                 = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    pipelineBuilder.myScissor.offset                = { 0, 0 };
    pipelineBuilder.myScissor.extent                = aCore.mySwapchainCtx.mySwapChainExtent;
    pipelineBuilder.myRasterizer                    = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE );
    pipelineBuilder.myMultisampling                 = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
    pipelineBuilder.myDepthStencil                  = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthWriteEnable = VK_FALSE;
    pipelineBuilder.myDepthStencil.depthTestEnable  = VK_FALSE;
    pipelineBuilder.myColorBlendAttachment          = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                = aLayout;
    pipelineBuilder.SetDefaultDynamicStates();

    VkPipeline pipeline = pipelineBuilder.BuildPipeline( aCore.myRhi.myDeviceCtx.myDevice, aRenderPass, aCore.myRhi.myDeviceCtx.myPipelineCache, nullptr );

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
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

    const std::string tonemapVertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapVertSpv );
    const std::string tonemapFragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kTonemapFragSpv );
    state.myTonemapPipeline           = BuildTonemapPipeline( aCore, aCore.mySwapchainCtx.myRenderPass, state.myTonemapPipelineLayout, tonemapVertPath, tonemapFragPath );

    const std::string thresholdPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomThresholdSpv );
    const std::string blurPath      = UtilLoader::ResolvePath( aCore.EngineConfig(), kBloomBlurSpv );
    state.myBloomThresholdPipeline  = BuildComputePipeline( aCore, state.myBloomThresholdPipelineLayout, thresholdPath );
    state.myBloomBlurPipeline       = BuildComputePipeline( aCore, state.myBloomBlurPipelineLayout, blurPath );

    const std::string taaPath  = UtilLoader::ResolvePath( aCore.EngineConfig(), kTaaResolveSpv );
    state.myTaaResolvePipeline = BuildComputePipeline( aCore, state.myTaaResolvePipelineLayout, taaPath );
}

void RebuildResources( Vk_Renderer& aCore ) {
    if ( aCore.myRhi.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }

    // Extent-scoped only (RP/FB). Sampler + descriptor pool live for pass lifetime — never flush them here.
    aCore.myPostProcessState.myDeletionQueue.flush();
    Vk_PostProcessPassDetail::ResetImageLayouts();

    Vk_PostProcessState& state = aCore.myPostProcessState;
    DestroyTexture( aCore, state.mySceneColor );
    DestroyTexture( aCore, state.myTaaResolved );
    DestroyTexture( aCore, state.myTaaHistory[ 0 ] );
    DestroyTexture( aCore, state.myTaaHistory[ 1 ] );
    DestroyTexture( aCore, state.myBloomPing );
    DestroyTexture( aCore, state.myBloomPong );
    state.myHybridRenderPass     = VK_NULL_HANDLE;
    state.myHybridFramebuffer    = VK_NULL_HANDLE;
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

    if ( aCore.myPostProcessState.myTonemapPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( aCore.myRhi.myDeviceCtx.myDevice, aCore.myPostProcessState.myTonemapPipeline, nullptr );
        aCore.myPostProcessState.myTonemapPipeline = VK_NULL_HANDLE;
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

VkImageMemoryBarrier SceneColorBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
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

void RecordBloom( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;

    if ( !post.myBloomEnabled ) {
        return;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gBloomPingLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gBloomPingLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else if ( oldLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myBloomPing.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    if ( Vk_PostProcessPassDetail::gBloomPongLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gBloomPongLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        else if ( oldLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myBloomPong.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPongLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    BloomThresholdPushConstants thresholdPush{ post.myBloomThreshold, 0.f, 0.f, 0.f };
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomThresholdPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomThresholdPipelineLayout, 0, 1, &state.myBloomThresholdDescriptorSets[ aFrameIndex ],
                             0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myBloomThresholdPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( thresholdPush ), &thresholdPush );

    const VkExtent2D bloomExtent = BloomExtent( aCore.mySwapchainCtx.mySwapChainExtent );
    vkCmdDispatch( aCommandBuffer, ( bloomExtent.width + 7 ) / 8, ( bloomExtent.height + 7 ) / 8, 1 );

    auto runBlurPass = [ & ]( bool aHorizontal ) {
        BloomBlurPushConstants blurPush{ aHorizontal ? 1u : 0u, aHorizontal ? 0u : 1u };
        VkDescriptorSet        set = aHorizontal ? state.myBloomBlurHorizDescriptorSets[ aFrameIndex ] : state.myBloomBlurVertDescriptorSets[ aFrameIndex ];
        vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomBlurPipeline );
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myBloomBlurPipelineLayout, 0, 1, &set, 0, nullptr );
        vkCmdPushConstants( aCommandBuffer, state.myBloomBlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( blurPush ), &blurPush );
        vkCmdDispatch( aCommandBuffer, ( bloomExtent.width + 7 ) / 8, ( bloomExtent.height + 7 ) / 8, 1 );
    };

    runBlurPass( true );

    VkMemoryBarrier blurDep{};
    blurDep.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blurDep.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    blurDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &blurDep, 0, nullptr, 0, nullptr );

    runBlurPass( false );

    // Two-pass blur ends in ping; tonemap samples bloom ping.
    VkImageMemoryBarrier bloomBarrier = SceneColorBarrier( state.myBloomPing.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                           VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &bloomBarrier );
    Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void RecordTaaResolve( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_PostProcessState&    state = aCore.myPostProcessState;
    const Gfx_PostSettings& post  = aCore.myPostSettings;
    if ( !post.myTaaEnabled ) {
        state.myTaaHistoryReady = false;
        return;
    }

    if ( !aCore.myTemporalState.myHistoryValid ) {
        state.myTaaHistoryReady = false;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    const uint32_t writeIndex = state.myTaaHistoryWriteIndex;
    const uint32_t readIndex  = 1u - writeIndex;

    if ( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ];
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_GENERAL ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier =
            SceneColorBarrier( state.myTaaHistory[ readIndex ].Image(), oldLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, srcAccess, VK_ACCESS_SHADER_READ_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaHistoryLayouts[ readIndex ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gTaaResolvedLayout != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaResolvedLayout;
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if ( oldLayout == VK_IMAGE_LAYOUT_GENERAL ) {
            srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
            srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier = SceneColorBarrier( state.myTaaResolved.Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaResolvedLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    TaaResolvePushConstants push{};
    push.historyBlend  = post.myTaaBlend;
    push.historyValid  = ( aCore.myTemporalState.myHistoryValid && state.myTaaHistoryReady ) ? 1.0f : 0.0f;
    push.varianceGamma = post.myTaaVarianceGamma;
    push.sharpen       = post.myTaaSharpen;

    UpdateTaaResolveDescriptorSet( aCore, aFrameIndex );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTaaResolvePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myTaaResolvePipelineLayout, 0, 1, &state.myTaaResolveDescriptorSets[ aFrameIndex ], 0,
                             nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myTaaResolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

    const VkExtent2D extent = aCore.mySwapchainCtx.mySwapChainExtent;
    vkCmdDispatch( aCommandBuffer, ( extent.width + 7 ) / 8, ( extent.height + 7 ) / 8, 1 );

    VkMemoryBarrier dep{};
    dep.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    dep.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &dep, 0,
                          nullptr, 0, nullptr );

    VkImageMemoryBarrier resolvedToRead0 = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &resolvedToRead0 );
    Vk_PostProcessPassDetail::gTaaResolvedLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Copy resolved -> history (writeIndex) for next frame.
    if ( Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] != VK_IMAGE_LAYOUT_GENERAL ) {
        const VkImageLayout  oldLayout = Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ];
        VkAccessFlags        srcAccess = 0;
        VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if ( oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
            srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        VkImageMemoryBarrier barrier =
            SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), oldLayout, VK_IMAGE_LAYOUT_GENERAL, srcAccess, VK_ACCESS_SHADER_WRITE_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount     = 1;
    copy.dstSubresource                = copy.srcSubresource;
    copy.extent                        = { extent.width, extent.height, 1 };
    VkImageMemoryBarrier resolvedToSrc = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT );
    VkImageMemoryBarrier historyToDst  = SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT );
    std::array< VkImageMemoryBarrier, 2 > toCopy = { resolvedToSrc, historyToDst };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toCopy.size() ), toCopy.data() );

    vkCmdCopyImage( aCommandBuffer, state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, state.myTaaHistory[ writeIndex ].Image(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );

    VkImageMemoryBarrier resolvedToRead = SceneColorBarrier( state.myTaaResolved.Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                             VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT );
    VkImageMemoryBarrier historyToRead  = SceneColorBarrier( state.myTaaHistory[ writeIndex ].Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    std::array< VkImageMemoryBarrier, 2 > toReadBarriers = { resolvedToRead, historyToRead };
    vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                          nullptr, static_cast< uint32_t >( toReadBarriers.size() ), toReadBarriers.data() );

    Vk_PostProcessPassDetail::gTaaResolvedLayout               = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Vk_PostProcessPassDetail::gTaaHistoryLayouts[ writeIndex ] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    state.myTaaHistoryWriteIndex                               = 1u - state.myTaaHistoryWriteIndex;
    state.myTaaHistoryReady                                    = true;
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
    if ( state.myTonemapPipeline != VK_NULL_HANDLE ) {
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
    DestroyTexture( aCore, state.mySceneColor );
    DestroyTexture( aCore, state.myTaaResolved );
    DestroyTexture( aCore, state.myTaaHistory[ 0 ] );
    DestroyTexture( aCore, state.myTaaHistory[ 1 ] );
    DestroyTexture( aCore, state.myBloomPing );
    DestroyTexture( aCore, state.myBloomPong );
    state.myHybridRenderPass  = VK_NULL_HANDLE;
    state.myHybridFramebuffer = VK_NULL_HANDLE;
    state.myInitialized       = false;
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

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex ) {
    Vk_PostProcessState& state = aCore.myPostProcessState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT ) {
        return;
    }

    RecordTaaResolve( aCore, aCommandBuffer, aFrameIndex );
    RecordBloom( aCore, aCommandBuffer, aFrameIndex );

    const Gfx_PostSettings& post = aCore.myPostSettings;
    if ( !post.myBloomEnabled && Vk_PostProcessPassDetail::gBloomPingLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        const VkImageLayout        oldLayout = Vk_PostProcessPassDetail::gBloomPingLayout;
        VkImageMemoryBarrier       barrier = SceneColorBarrier( state.myBloomPing.Image(), oldLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT );
        const VkPipelineStageFlags srcStage = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        vkCmdPipelineBarrier( aCommandBuffer, srcStage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );
        Vk_PostProcessPassDetail::gBloomPingLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        // Hybrid RP finalLayout is already SHADER_READ_ONLY; Prefer MarkSceneColorShaderRead after resolve.
        // Avoid UNDEFINED→READ (discards contents). Only barrier when we know a real prior layout.
        if ( Vk_PostProcessPassDetail::gSceneColorLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
            VkImageMemoryBarrier barrier = SceneColorBarrier( state.mySceneColor.Image(), Vk_PostProcessPassDetail::gSceneColorLayout,
                                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
            vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &barrier );
        }
        Vk_PostProcessPassDetail::gSceneColorLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Refresh every frame so ImGui TAA toggle cannot leave tonemap bound to an unwritten taaResolved.
    UpdateTonemapDescriptorSet( aCore, aFrameIndex );

    VkRenderPassBeginInfo tonemapBegin{};
    tonemapBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    tonemapBegin.renderPass        = aCore.mySwapchainCtx.myRenderPass;
    tonemapBegin.framebuffer       = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ aImageIndex ];
    tonemapBegin.renderArea.offset = { 0, 0 };
    tonemapBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 2 > clears{};
    clears[ 0 ].color            = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clears[ 1 ].depthStencil     = { 1.0f, 0 };
    tonemapBegin.clearValueCount = static_cast< uint32_t >( clears.size() );
    tonemapBegin.pClearValues    = clears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &tonemapBegin, VK_SUBPASS_CONTENTS_INLINE );

    TonemapPushConstants push{};
    push.exposure       = post.myExposure;
    push.bloomIntensity = post.myBloomIntensity;
    push.tonemapEnabled = post.myTonemapEnabled ? 1u : 0u;
    push.bloomEnabled   = post.myBloomEnabled ? 1u : 0u;
    push.tonemapMode    = post.myTonemapMode != 0 ? 1u : 0u;

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myTonemapPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myTonemapPipelineLayout, 0, 1, &state.myTonemapDescriptorSets[ aFrameIndex ], 0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myTonemapPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=Tonemap" );
    }
    vkCmdDraw( aCommandBuffer, 3, 1, 0, 0 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

}  // namespace Vk_PostProcessPass

// Module: Vk_GBufferPass — FG v0 slice 1 (HybridDeferred preset).
// Chain: GBufferOpaque -> ClusterBuild -> CompositeAlbedo -> swapchain. DeferredLighting deferred (slice 3).
// Format policy: Docs/s3-fg-s1-preset-gbuffer_Plan.md (not EngineArchitecture until locked).
#include "Vk_GBufferPass.h"

#include "../App/DebugUIState.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include "Vk_ClusterBuildPass.h"
#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_RenderBackend.h"
#include "Vk_ScenePasses.h"

#include "Vk_Types.h"

#include <array>
#include <vector>

namespace {

constexpr const char* kGBufferVertSpv   = "VulkanDesktop/Shader_Generated/GBufferVert.spv";
constexpr const char* kGBufferFragSpv   = "VulkanDesktop/Shader_Generated/GBufferFrag.spv";
constexpr const char* kCompositeVertSpv = "VulkanDesktop/Shader_Generated/CompositeAlbedoVert.spv";
constexpr const char* kCompositeFragSpv = "VulkanDesktop/Shader_Generated/CompositeAlbedoFrag.spv";

constexpr VkFormat kAlbedoFormat          = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat kNormalRoughnessFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

void DestroyPipelines( Vk_Core& aCore ) {
    const VkDevice device = aCore.myDeviceCtx.myDevice;
    if ( device == VK_NULL_HANDLE ) {
        return;
    }
    if ( aCore.myGBufferState.myGBufferPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myGBufferState.myGBufferPipeline, nullptr );
        aCore.myGBufferState.myGBufferPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myGBufferState.myCompositePipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myGBufferState.myCompositePipeline, nullptr );
        aCore.myGBufferState.myCompositePipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myGBufferState.myCompositePipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, aCore.myGBufferState.myCompositePipelineLayout, nullptr );
        aCore.myGBufferState.myCompositePipelineLayout = VK_NULL_HANDLE;
    }
    if ( aCore.myGBufferState.myCompositeSetLayout != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( device, aCore.myGBufferState.myCompositeSetLayout, nullptr );
        aCore.myGBufferState.myCompositeSetLayout = VK_NULL_HANDLE;
    }
    if ( aCore.myGBufferState.myCompositeDescriptorPool != VK_NULL_HANDLE ) {
        vkDestroyDescriptorPool( device, aCore.myGBufferState.myCompositeDescriptorPool, nullptr );
        aCore.myGBufferState.myCompositeDescriptorPool = VK_NULL_HANDLE;
    }
    aCore.myGBufferState.myCompositeDescriptorSet = VK_NULL_HANDLE;
    if ( aCore.myGBufferState.myCompositeSampler != VK_NULL_HANDLE ) {
        vkDestroySampler( device, aCore.myGBufferState.myCompositeSampler, nullptr );
        aCore.myGBufferState.myCompositeSampler = VK_NULL_HANDLE;
    }
}

VkPipeline BuildGBufferPipeline( Vk_Core& aCore, VkRenderPass aRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo = vertexInputInfo;
    pipelineBuilder.myInputAssembly   = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport        = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    pipelineBuilder.myScissor.offset  = { 0, 0 };
    pipelineBuilder.myScissor.extent  = aCore.mySwapchainCtx.mySwapChainExtent;
    pipelineBuilder.myRasterizer      = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL );
    pipelineBuilder.myMultisampling   = VkInit::Pipeline_MultisampleCreateInfo( VK_SAMPLE_COUNT_1_BIT );
    pipelineBuilder.myDepthStencil    = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myPipelineLayout  = aLayout;
    pipelineBuilder.SetDefaultDynamicStates();

    std::vector< VkPipelineColorBlendAttachmentState > blendAttachments = {
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
        VkInit::Pipeline_ColorBlendAttachment( VK_FALSE ),
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = VkInit::Pipeline_ColorBlendCreateInfo( blendAttachments );

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &pipelineBuilder.myViewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &pipelineBuilder.myScissor;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast< uint32_t >( pipelineBuilder.myShaderStages.size() );
    pipelineInfo.pStages             = pipelineBuilder.myShaderStages.data();
    pipelineInfo.pVertexInputState   = &pipelineBuilder.myVertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineBuilder.myInputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &pipelineBuilder.myRasterizer;
    pipelineInfo.pMultisampleState   = &pipelineBuilder.myMultisampling;
    pipelineInfo.pDepthStencilState  = &pipelineBuilder.myDepthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &pipelineBuilder.myDynamicState;
    pipelineInfo.layout              = aLayout;
    pipelineInfo.renderPass          = aRenderPass;
    pipelineInfo.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    UtilVulkanResult::ThrowOnFailure( vkCreateGraphicsPipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ),
                                      "vkCreateGraphicsPipelines GBuffer" );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
}

VkPipeline BuildCompositePipeline( Vk_Core& aCore, VkRenderPass aSwapchainRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkInit::Pipeline_VertexInputStateCreateInfo();
    Vk_PipelineBuilder                   pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo               = vertexInputInfo;
    pipelineBuilder.myInputAssembly                 = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    pipelineBuilder.myScissor.offset                = { 0, 0 };
    pipelineBuilder.myScissor.extent                = aCore.mySwapchainCtx.mySwapChainExtent;
    pipelineBuilder.myRasterizer                    = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL );
    pipelineBuilder.myMultisampling                 = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
    pipelineBuilder.myDepthStencil                  = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthWriteEnable = VK_FALSE;
    pipelineBuilder.myDepthStencil.depthTestEnable  = VK_FALSE;
    pipelineBuilder.myColorBlendAttachment          = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                = aLayout;
    pipelineBuilder.SetDefaultDynamicStates();

    VkPipeline pipeline = pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aSwapchainRenderPass, aCore.myDeviceCtx.myPipelineCache, nullptr );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
}

void CreateGBufferRenderPass( Vk_Core& aCore ) {
    VkAttachmentDescription albedo{};
    albedo.format         = kAlbedoFormat;
    albedo.samples        = VK_SAMPLE_COUNT_1_BIT;
    albedo.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedo.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    albedo.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedo.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    albedo.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalRoughness = albedo;
    normalRoughness.format                  = kNormalRoughnessFormat;

    VkAttachmentDescription depth{};
    depth.format         = aCore.FindDepthFormat();
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array< VkAttachmentReference, 2 > colorRefs{};
    colorRefs[ 0 ].attachment = 0;
    colorRefs[ 0 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[ 1 ].attachment = 1;
    colorRefs[ 1 ].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 2;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = static_cast< uint32_t >( colorRefs.size() );
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array< VkAttachmentDescription, 3 > attachments = { albedo, normalRoughness, depth };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    UtilVulkanResult::ThrowOnFailure( vkCreateRenderPass( aCore.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &aCore.myGBufferState.myRenderPass ),
                                      "vkCreateRenderPass GBuffer" );

    const VkDevice     device     = aCore.myDeviceCtx.myDevice;
    const VkRenderPass renderPass = aCore.myGBufferState.myRenderPass;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, renderPass ]() { vkDestroyRenderPass( device, renderPass, nullptr ); } );
}

void CreateGBufferImages( Vk_Core& aCore ) {
    const VkExtent2D extent      = aCore.mySwapchainCtx.mySwapChainExtent;
    const VkFormat   depthFormat = aCore.FindDepthFormat();

    auto createColorTarget = [ & ]( Gfx_Texture& aTexture, VkFormat aFormat ) {
        aCore.CreateImage( extent, aFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                           VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
        aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT );

        const VkDevice      device    = aCore.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myDeviceCtx.myAllocator;
        const VkImageView   view      = aTexture.ImageView();
        const VkImage       image     = aTexture.Image();
        const VmaAllocation alloc     = aTexture.Allocation();
        aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, allocator, view, image, alloc ]() {
            vkDestroyImageView( device, view, nullptr );
            vmaDestroyImage( allocator, image, alloc );
        } );
    };

    createColorTarget( aCore.myGBufferState.myAlbedo, kAlbedoFormat );
    createColorTarget( aCore.myGBufferState.myNormalRoughness, kNormalRoughnessFormat );

    aCore.CreateImage( extent, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT,
                       aCore.myGBufferState.myDepth.AllocImage() );
    aCore.myGBufferState.myDepth.ImageView() = aCore.CreateImageView( aCore.myGBufferState.myDepth.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );

    {
        const VkDevice      device    = aCore.myDeviceCtx.myDevice;
        const VmaAllocator  allocator = aCore.myDeviceCtx.myAllocator;
        const VkImageView   view      = aCore.myGBufferState.myDepth.ImageView();
        const VkImage       image     = aCore.myGBufferState.myDepth.Image();
        const VmaAllocation alloc     = aCore.myGBufferState.myDepth.Allocation();
        aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, allocator, view, image, alloc ]() {
            vkDestroyImageView( device, view, nullptr );
            vmaDestroyImage( allocator, image, alloc );
        } );
    }
}

void CreateGBufferFramebuffer( Vk_Core& aCore ) {
    std::array< VkImageView, 3 > attachments = { aCore.myGBufferState.myAlbedo.ImageView(), aCore.myGBufferState.myNormalRoughness.ImageView(),
                                                 aCore.myGBufferState.myDepth.ImageView() };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = aCore.myGBufferState.myRenderPass;
    framebufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = aCore.mySwapchainCtx.mySwapChainExtent.width;
    framebufferInfo.height          = aCore.mySwapchainCtx.mySwapChainExtent.height;
    framebufferInfo.layers          = 1;

    UtilVulkanResult::ThrowOnFailure( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &framebufferInfo, nullptr, &aCore.myGBufferState.myFramebuffer ),
                                      "vkCreateFramebuffer GBuffer" );

    const VkDevice      device      = aCore.myDeviceCtx.myDevice;
    const VkFramebuffer framebuffer = aCore.myGBufferState.myFramebuffer;
    aCore.myGBufferState.myDeletionQueue.pushFunction( [ device, framebuffer ]() { vkDestroyFramebuffer( device, framebuffer, nullptr ); } );
}

void CreateCompositeResources( Vk_Core& aCore, const std::string& aVertPath, const std::string& aFragPath ) {
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding         = 0;
    samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &samplerBinding;
    UtilVulkanResult::ThrowOnFailure( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &aCore.myGBufferState.myCompositeSetLayout ),
                                      "vkCreateDescriptorSetLayout composite" );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &aCore.myGBufferState.myCompositeSetLayout;
    UtilVulkanResult::ThrowOnFailure( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &aCore.myGBufferState.myCompositePipelineLayout ),
                                      "vkCreatePipelineLayout composite" );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    UtilVulkanResult::ThrowOnFailure( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &aCore.myGBufferState.myCompositeSampler ),
                                      "vkCreateSampler composite" );

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;
    UtilVulkanResult::ThrowOnFailure( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &aCore.myGBufferState.myCompositeDescriptorPool ),
                                      "vkCreateDescriptorPool composite" );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = aCore.myGBufferState.myCompositeDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &aCore.myGBufferState.myCompositeSetLayout;
    UtilVulkanResult::ThrowOnFailure( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &aCore.myGBufferState.myCompositeDescriptorSet ),
                                      "vkAllocateDescriptorSets composite" );

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler     = aCore.myGBufferState.myCompositeSampler;
    imageInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = aCore.myGBufferState.myCompositeDescriptorSet;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, 1, &write, 0, nullptr );

    aCore.myGBufferState.myCompositePipeline =
        BuildCompositePipeline( aCore, aCore.mySwapchainCtx.myRenderPass, aCore.myGBufferState.myCompositePipelineLayout, aVertPath, aFragPath );
}

// Rebind albedo view after extent rebuild (descriptor set allocation is stable; image view handle changes).
void UpdateCompositeDescriptor( Vk_Core& aCore ) {
    if ( aCore.myGBufferState.myCompositeDescriptorSet == VK_NULL_HANDLE ) {
        return;
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler     = aCore.myGBufferState.myCompositeSampler;
    imageInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = aCore.myGBufferState.myCompositeDescriptorSet;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, 1, &write, 0, nullptr );
}

void RebuildResources( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myDevice == VK_NULL_HANDLE || aCore.mySwapchainCtx.mySwapChainExtent.width == 0 ) {
        return;
    }

    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();

    const std::string vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferVertSpv );
    const std::string fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kGBufferFragSpv );
    const std::string compVert = UtilLoader::ResolvePath( aCore.EngineConfig(), kCompositeVertSpv );
    const std::string compFrag = UtilLoader::ResolvePath( aCore.EngineConfig(), kCompositeFragSpv );

    CreateGBufferRenderPass( aCore );
    CreateGBufferImages( aCore );
    CreateGBufferFramebuffer( aCore );
    CreateCompositeResources( aCore, compVert, compFrag );

    aCore.myGBufferState.myGBufferPipeline = BuildGBufferPipeline( aCore, aCore.myGBufferState.myRenderPass, aCore.mySceneGpuCtx.myPipelineLayout, vertPath, fragPath );
}

}  // namespace

namespace Vk_GBufferPass {

bool IsActive( const Vk_Core& aCore ) {
    return Gfx_RenderPreset::IsHybridDeferred( aCore.EngineConfig().GetRenderPresetName() );
}

void Destroy( Vk_Core& aCore ) {
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
    }
    DestroyPipelines( aCore );
    aCore.myGBufferState.myDeletionQueue.flush();
    aCore.myGBufferState.myFramebuffer = VK_NULL_HANDLE;
    aCore.myGBufferState.myRenderPass  = VK_NULL_HANDLE;
    aCore.myGBufferState.myInitialized = false;
}

void RecreateForExtent( Vk_Core& aCore ) {
    // ForwardLit never calls Init — skip swapchain resize work when hybrid path is inactive.
    if ( !aCore.myGBufferState.myInitialized ) {
        return;
    }
    RebuildResources( aCore );
}

void Init( Vk_Core& aCore ) {
    if ( aCore.myGBufferState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_GBufferPass::Init (slice 1)." );
    RebuildResources( aCore );
    aCore.myGBufferState.myInitialized = true;
}

void RecordFrame( Vk_Core& aCore, const DebugUIState& aDebugUI, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex,
                  const std::array< VkViewport, kGfxMaxRenderViews >& aViewports, const std::array< VkRect2D, kGfxMaxRenderViews >& aScissors,
                  const std::array< VkDescriptorSet, kGfxMaxRenderViews >& aFrameDescriptors, uint32_t aViewCount,
                  const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets ) {

    static bool sChainLoggedOnce      = false;
    static bool sBindlessFallbackOnce = false;
    static bool sMultiViewWarnOnce    = false;
    static bool sTransparentSkipOnce  = false;

    if ( !sChainLoggedOnce ) {
        UtilLogger::Info( "FG", "HybridDeferred: GBufferOpaque -> ClusterBuild -> CompositeAlbedo (DeferredLighting deferred)" );
        sChainLoggedOnce = true;
    }

    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        if ( !sBindlessFallbackOnce ) {
            UtilLogger::Warn( "FG", "HybridDeferred slice1 is batch-only; falling back to ForwardLit record." );
            sBindlessFallbackOnce = true;
        }
        Vk_ScenePasses::RecordForwardLit( aCore, aDebugUI, aCommandBuffer, anImageIndex, aViewports, aScissors, aFrameDescriptors, aViewCount, aViewPackets );
        return;
    }

    if ( aViewCount > 1 && !sMultiViewWarnOnce ) {
        UtilLogger::Warn( "FG", "HybridDeferred slice1 uses view 0 only." );
        sMultiViewWarnOnce = true;
    }

    if ( !sTransparentSkipOnce ) {
        UtilLogger::Info( "FG", "HybridDeferred slice1: transparent pass skipped." );
        sTransparentSkipOnce = true;
    }

    const bool legacyDirectDraw = aCore.EngineConfig().GetLegacyDirectDraw();
    const bool gpuCullRecord    = aCore.EngineConfig().GetGpuCullEnabled() && !legacyDirectDraw;
    const bool emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();

    Vk_FrameData&  frame          = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
    const VkBuffer indirectBuffer = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;

    VkRenderPassBeginInfo gbufferBegin{};
    gbufferBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gbufferBegin.renderPass        = aCore.myGBufferState.myRenderPass;
    gbufferBegin.framebuffer       = aCore.myGBufferState.myFramebuffer;
    gbufferBegin.renderArea.offset = { 0, 0 };
    gbufferBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 3 > gbufferClears{};
    gbufferClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    gbufferClears[ 1 ].color        = { { 0.0f, 0.0f, 1.0f, 0.5f } };
    gbufferClears[ 2 ].depthStencil = { 1.0f, 0 };
    gbufferBegin.clearValueCount    = static_cast< uint32_t >( gbufferClears.size() );
    gbufferBegin.pClearValues       = gbufferClears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &gbufferBegin, VK_SUBPASS_CONTENTS_INLINE );

    const uint32_t viewIndex = 0;
    if ( viewIndex < aViewCount ) {
        const Gfx_FrameRenderPacket& packet = aViewPackets[ viewIndex ];
        aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex ], aScissors[ viewIndex ] );
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myPipelineLayout, VkDescriptorPolicy::kSetFrame, 1,
                                 &aFrameDescriptors[ viewIndex ], 0, nullptr );

        if ( Vk_RenderBackend::ValidateFramePacket( packet ) && !aDebugUI.myRenderDebug.mySkipOpaquePass ) {
            if ( emitDebugLabels ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=GBufferOpaque" );
            }
            Vk_ScenePasses::RecordOpaquePacketDraws( aCore, aCommandBuffer, packet.myOpaquePass, packet.myDrawBufferBaseIndex, indirectBuffer, gpuCullRecord, legacyDirectDraw,
                                                     emitDebugLabels, aCore.myGBufferState.myGBufferPipeline );
            if ( emitDebugLabels ) {
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }
    }

    vkCmdEndRenderPass( aCommandBuffer );

    Vk_ClusterBuildPass::RecordDispatch( aCore, aCommandBuffer, aCore.myFrameCtx.myCurrentFrame );

    UpdateCompositeDescriptor( aCore );

    VkRenderPassBeginInfo swapBegin{};
    swapBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    swapBegin.renderPass        = aCore.mySwapchainCtx.myRenderPass;
    swapBegin.framebuffer       = aCore.mySwapchainCtx.mySwapChainFrameBuffers[ anImageIndex ];
    swapBegin.renderArea.offset = { 0, 0 };
    swapBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
    std::array< VkClearValue, 2 > swapClears{};
    swapClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    swapClears[ 1 ].depthStencil = { 1.0f, 0 };
    swapBegin.clearValueCount    = static_cast< uint32_t >( swapClears.size() );
    swapBegin.pClearValues       = swapClears.data();

    vkCmdBeginRenderPass( aCommandBuffer, &swapBegin, VK_SUBPASS_CONTENTS_INLINE );

    aCore.SetGraphicsDynamicState( aCommandBuffer, aViewports[ viewIndex < aViewCount ? viewIndex : 0 ], aScissors[ viewIndex < aViewCount ? viewIndex : 0 ] );
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myGBufferState.myCompositePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.myGBufferState.myCompositePipelineLayout, 0, 1,
                             &aCore.myGBufferState.myCompositeDescriptorSet, 0, nullptr );
    if ( emitDebugLabels ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=CompositeAlbedo" );
    }
    vkCmdDraw( aCommandBuffer, 3, 1, 0, 0 );
    if ( emitDebugLabels ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    vkCmdEndRenderPass( aCommandBuffer );
}

}  // namespace Vk_GBufferPass

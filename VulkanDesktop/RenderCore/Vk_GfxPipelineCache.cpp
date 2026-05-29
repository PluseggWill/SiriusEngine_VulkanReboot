#include "Vk_GfxPipelineCache.h"

#include "Vk_Core.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_PipelineDiagnostics.h"

#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include <array>

extern std::string vertShaderPath;
extern std::string fragShaderPath;
extern std::string bindlessFragShaderPath;

void Vk_GfxPipelineCache::InitScenePipelines( Vk_Core& aCore ) {
    DestroyScenePipelines( aCore );
    CreateGfxPipeline( aCore );
    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        CreateBindlessGfxPipelines( aCore );
    }
}

void Vk_GfxPipelineCache::DestroyScenePipelines( Vk_Core& aCore ) {
    if ( aCore.myDevice == VK_NULL_HANDLE ) {
        return;
    }

    const VkDevice device = aCore.myDevice;

    if ( aCore.myBasicPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myBasicPipeline, nullptr );
        aCore.myBasicPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myTransparentPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myTransparentPipeline, nullptr );
        aCore.myTransparentPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.myPipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, aCore.myPipelineLayout, nullptr );
        aCore.myPipelineLayout = VK_NULL_HANDLE;
    }
    if ( aCore.myBasicPipelineBindless != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myBasicPipelineBindless, nullptr );
        aCore.myBasicPipelineBindless = VK_NULL_HANDLE;
    }
    if ( aCore.myTransparentPipelineBindless != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.myTransparentPipelineBindless, nullptr );
        aCore.myTransparentPipelineBindless = VK_NULL_HANDLE;
    }
}

void Vk_GfxPipelineCache::CreateGfxPipeline( Vk_Core& aCore ) {
    UtilLogger::Info( "PIPELINE", "Creating graphics pipeline." );
    VkShaderModule vertShaderModule = aCore.CreateShaderModule( vertShaderPath );
    VkShaderModule fragShaderModule = aCore.CreateShaderModule( fragShaderPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    VkViewport                             viewport      = VkInit::ViewportCreateInfo( aCore.mySwapChainExtent );
    VkRect2D                               scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = aCore.mySwapChainExtent;

    VkPipelineRasterizationStateCreateInfo rasterizer   = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );
    VkPipelineMultisampleStateCreateInfo   multisampling = VkInit::Pipeline_MultisampleCreateInfo( aCore.myMSAASamples );
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo = VkInit::Pipeline_DepthStencilCreateInfo();
    VkPipelineColorBlendAttachmentState    colorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );

    const std::array< VkDescriptorSetLayout, 3 > setLayouts = { aCore.myGlobalSetLayout, aCore.myMaterialSetLayout, aCore.myObjectSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutCreateInfo.setLayoutCount             = static_cast< uint32_t >( setLayouts.size() );
    pipelineLayoutCreateInfo.pSetLayouts                = setLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges        = nullptr;

    UtilLogger::Info( "PIPELINE", "Creating pipeline layout: setCount=3 (frame, material, object dynamic)." );
    UtilVulkanResult::ThrowOnFailure( vkCreatePipelineLayout( aCore.myDevice, &pipelineLayoutCreateInfo, nullptr, &aCore.myPipelineLayout ), "vkCreatePipelineLayout" );

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main" );
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main" );

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( vertShaderStageInfo );
    pipelineBuilder.myShaderStages.push_back( fragShaderStageInfo );
    pipelineBuilder.myVertexInputInfo      = vertexInputInfo;
    pipelineBuilder.myInputAssembly        = inputAssembly;
    pipelineBuilder.myViewport             = viewport;
    pipelineBuilder.myScissor              = scissor;
    pipelineBuilder.myRasterizer           = rasterizer;
    pipelineBuilder.myMultisampling        = multisampling;
    pipelineBuilder.myDepthStencil         = depthStencilInfo;
    pipelineBuilder.myColorBlendAttachment = colorBlendAttachment;
    pipelineBuilder.SetDefaultDynamicStates();
    pipelineBuilder.myPipelineLayout       = aCore.myPipelineLayout;

    Vk_GraphicsPipelineBuildInfo pipelineDiag{};
    pipelineDiag.myLabel                   = "basic-lit";
    pipelineDiag.myVertShaderPath          = vertShaderPath.c_str();
    pipelineDiag.myFragShaderPath          = fragShaderPath.c_str();
    pipelineDiag.myPipelineLayoutSetCount  = pipelineLayoutCreateInfo.setLayoutCount;
    pipelineDiag.myPipelineLayoutPushCount = 0;
    pipelineDiag.myColorFormat             = aCore.mySwapChainImageFormat;
    pipelineDiag.myDepthFormat             = aCore.FindDepthFormat();

    aCore.myBasicPipeline = pipelineBuilder.BuildPipeline( aCore.myDevice, aCore.myRenderPass, &pipelineDiag );

    pipelineBuilder.myDepthStencil         = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachmentAlpha();
    Vk_GraphicsPipelineBuildInfo transparentDiag = pipelineDiag;
    transparentDiag.myLabel                      = "basic-lit-transparent";
    aCore.myTransparentPipeline                  = pipelineBuilder.BuildPipeline( aCore.myDevice, aCore.myRenderPass, &transparentDiag );

    vkDestroyShaderModule( aCore.myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( aCore.myDevice, fragShaderModule, nullptr );
}

void Vk_GfxPipelineCache::CreateBindlessGfxPipelines( Vk_Core& aCore ) {
    UtilLogger::Info( "PIPELINE", "Creating bindless graphics pipelines." );

    VkShaderModule vertShaderModule = aCore.CreateShaderModule( vertShaderPath );
    VkShaderModule fragShaderModule = aCore.CreateShaderModule( bindlessFragShaderPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    VkViewport                             viewport      = VkInit::ViewportCreateInfo( aCore.mySwapChainExtent );
    VkRect2D                               scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = aCore.mySwapChainExtent;

    VkPipelineRasterizationStateCreateInfo rasterizer = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );
    VkPipelineMultisampleStateCreateInfo   multisampling = VkInit::Pipeline_MultisampleCreateInfo( aCore.myMSAASamples );
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo = VkInit::Pipeline_DepthStencilCreateInfo();

    VkPipelineShaderStageCreateInfo vertStage = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, "main" );
    VkPipelineShaderStageCreateInfo fragStage = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, "main" );

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( vertStage );
    pipelineBuilder.myShaderStages.push_back( fragStage );
    pipelineBuilder.myVertexInputInfo      = vertexInputInfo;
    pipelineBuilder.myInputAssembly        = inputAssembly;
    pipelineBuilder.myViewport             = viewport;
    pipelineBuilder.myScissor              = scissor;
    pipelineBuilder.myRasterizer           = rasterizer;
    pipelineBuilder.myMultisampling        = multisampling;
    pipelineBuilder.myDepthStencil         = depthStencilInfo;
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.SetDefaultDynamicStates();
    pipelineBuilder.myPipelineLayout       = aCore.myBindlessPipelineLayout;

    Vk_GraphicsPipelineBuildInfo diag{};
    diag.myLabel          = "basic-lit-bindless";
    diag.myVertShaderPath = vertShaderPath.c_str();
    diag.myFragShaderPath = bindlessFragShaderPath.c_str();
    diag.myColorFormat    = aCore.mySwapChainImageFormat;
    diag.myDepthFormat    = aCore.FindDepthFormat();

    aCore.myBasicPipelineBindless = pipelineBuilder.BuildPipeline( aCore.myDevice, aCore.myRenderPass, &diag );

    pipelineBuilder.myDepthStencil         = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachmentAlpha();
    diag.myLabel                           = "basic-lit-bindless-transparent";
    aCore.myTransparentPipelineBindless    = pipelineBuilder.BuildPipeline( aCore.myDevice, aCore.myRenderPass, &diag );

    vkDestroyShaderModule( aCore.myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( aCore.myDevice, fragShaderModule, nullptr );
}

#include "Vk_GfxPipelineCache.h"

#include "../Gfx/Gfx_RenderPreset.h"
#include "../Util/Util_EngineConfig.h"
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
    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        CreateBindlessGfxPipelines( aCore );
    }
    CreateHybridResolveGfxPipelines( aCore );
}

void Vk_GfxPipelineCache::DestroyScenePipelines( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myDevice == VK_NULL_HANDLE ) {
        return;
    }

    const VkDevice device = aCore.myDeviceCtx.myDevice;

    if ( aCore.mySceneGpuCtx.myBasicPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myBasicPipeline, nullptr );
        aCore.mySceneGpuCtx.myBasicPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myTransparentPipeline != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myTransparentPipeline, nullptr );
        aCore.mySceneGpuCtx.myTransparentPipeline = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myPipelineLayout != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( device, aCore.mySceneGpuCtx.myPipelineLayout, nullptr );
        aCore.mySceneGpuCtx.myPipelineLayout = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myBasicPipelineBindless != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myBasicPipelineBindless, nullptr );
        aCore.mySceneGpuCtx.myBasicPipelineBindless = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myTransparentPipelineBindless != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myTransparentPipelineBindless, nullptr );
        aCore.mySceneGpuCtx.myTransparentPipelineBindless = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myBasicPipelineHybridResolve != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myBasicPipelineHybridResolve, nullptr );
        aCore.mySceneGpuCtx.myBasicPipelineHybridResolve = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myBasicPipelineBindlessHybridResolve != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myBasicPipelineBindlessHybridResolve, nullptr );
        aCore.mySceneGpuCtx.myBasicPipelineBindlessHybridResolve = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myTransparentPipelineHybridResolve != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myTransparentPipelineHybridResolve, nullptr );
        aCore.mySceneGpuCtx.myTransparentPipelineHybridResolve = VK_NULL_HANDLE;
    }
    if ( aCore.mySceneGpuCtx.myTransparentPipelineBindlessHybridResolve != VK_NULL_HANDLE ) {
        vkDestroyPipeline( device, aCore.mySceneGpuCtx.myTransparentPipelineBindlessHybridResolve, nullptr );
        aCore.mySceneGpuCtx.myTransparentPipelineBindlessHybridResolve = VK_NULL_HANDLE;
    }
}

void Vk_GfxPipelineCache::CreateGfxPipeline( Vk_Core& aCore ) {
    UtilLogger::Info( "PIPELINE", "Creating graphics pipeline." );
    VkShaderModule vertShaderModule = aCore.CreateShaderModule( vertShaderPath );
    VkShaderModule fragShaderModule = aCore.CreateShaderModule( fragShaderPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    VkViewport                             viewport      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    VkRect2D                               scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = aCore.mySwapchainCtx.mySwapChainExtent;

    VkPipelineRasterizationStateCreateInfo rasterizer           = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );
    VkPipelineMultisampleStateCreateInfo   multisampling        = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo     = VkInit::Pipeline_DepthStencilCreateInfo();
    VkPipelineColorBlendAttachmentState    colorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );

    const std::array< VkDescriptorSetLayout, 3 > setLayouts               = { aCore.mySceneGpuCtx.myGlobalSetLayout, aCore.mySceneGpuCtx.myMaterialSetLayout,
                                                                              aCore.mySceneGpuCtx.myObjectSetLayout };
    VkPipelineLayoutCreateInfo                   pipelineLayoutCreateInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutCreateInfo.setLayoutCount                               = static_cast< uint32_t >( setLayouts.size() );
    pipelineLayoutCreateInfo.pSetLayouts                                  = setLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount                       = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges                          = nullptr;

    UtilLogger::Info( "PIPELINE", "Creating pipeline layout: setCount=3 (frame, material, object dynamic)." );
    UtilVulkanResult::ThrowOnFailure( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutCreateInfo, nullptr, &aCore.mySceneGpuCtx.myPipelineLayout ),
                                      "vkCreatePipelineLayout" );

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
    pipelineBuilder.myPipelineLayout = aCore.mySceneGpuCtx.myPipelineLayout;

    Vk_GraphicsPipelineBuildInfo pipelineDiag{};
    pipelineDiag.myLabel                   = "basic-lit";
    pipelineDiag.myVertShaderPath          = vertShaderPath.c_str();
    pipelineDiag.myFragShaderPath          = fragShaderPath.c_str();
    pipelineDiag.myPipelineLayoutSetCount  = pipelineLayoutCreateInfo.setLayoutCount;
    pipelineDiag.myPipelineLayoutPushCount = 0;
    pipelineDiag.myColorFormat             = aCore.mySwapchainCtx.mySwapChainImageFormat;
    pipelineDiag.myDepthFormat             = aCore.FindDepthFormat();

    aCore.mySceneGpuCtx.myBasicPipeline =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myRenderPass, aCore.myDeviceCtx.myPipelineCache, &pipelineDiag );

    pipelineBuilder.myDepthStencil               = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
    pipelineBuilder.myColorBlendAttachment       = VkInit::Pipeline_ColorBlendAttachmentAlpha();
    Vk_GraphicsPipelineBuildInfo transparentDiag = pipelineDiag;
    transparentDiag.myLabel                      = "basic-lit-transparent";
    aCore.mySceneGpuCtx.myTransparentPipeline =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myRenderPass, aCore.myDeviceCtx.myPipelineCache, &transparentDiag );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragShaderModule, nullptr );
}

void Vk_GfxPipelineCache::CreateBindlessGfxPipelines( Vk_Core& aCore ) {
    UtilLogger::Info( "PIPELINE", "Creating bindless graphics pipelines." );

    VkShaderModule vertShaderModule = aCore.CreateShaderModule( vertShaderPath );
    VkShaderModule fragShaderModule = aCore.CreateShaderModule( bindlessFragShaderPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    VkViewport                             viewport      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    VkRect2D                               scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = aCore.mySwapchainCtx.mySwapChainExtent;

    VkPipelineRasterizationStateCreateInfo rasterizer       = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );
    VkPipelineMultisampleStateCreateInfo   multisampling    = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
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
    pipelineBuilder.myPipelineLayout = aCore.mySceneGpuCtx.myBindlessPipelineLayout;

    Vk_GraphicsPipelineBuildInfo diag{};
    diag.myLabel          = "basic-lit-bindless";
    diag.myVertShaderPath = vertShaderPath.c_str();
    diag.myFragShaderPath = bindlessFragShaderPath.c_str();
    diag.myColorFormat    = aCore.mySwapchainCtx.mySwapChainImageFormat;
    diag.myDepthFormat    = aCore.FindDepthFormat();

    aCore.mySceneGpuCtx.myBasicPipelineBindless =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );

    pipelineBuilder.myDepthStencil         = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachmentAlpha();
    diag.myLabel                           = "basic-lit-bindless-transparent";
    aCore.mySceneGpuCtx.myTransparentPipelineBindless =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragShaderModule, nullptr );
}

void Vk_GfxPipelineCache::CreateHybridResolveGfxPipelines( Vk_Core& aCore ) {
    // ForwardTransparent over copied G-buffer depth — separate render pass from myRenderPass (ImGui overlay RP follows).
    if ( !Gfx_RenderPreset::IsHybridDeferred( aCore.EngineConfig().GetRenderPresetName() ) || aCore.mySwapchainCtx.myHybridResolveRenderPass == VK_NULL_HANDLE ) {
        return;
    }

    UtilLogger::Info( "PIPELINE", "Creating hybrid-resolve transparent pipelines." );

    VkShaderModule vertShaderModule = aCore.CreateShaderModule( vertShaderPath );
    VkShaderModule fragShaderModule = aCore.CreateShaderModule( fragShaderPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    VkViewport                             viewport      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    VkRect2D                               scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = aCore.mySwapchainCtx.mySwapChainExtent;

    VkPipelineRasterizationStateCreateInfo rasterizer       = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );
    VkPipelineMultisampleStateCreateInfo   multisampling    = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
    VkPipelineDepthStencilStateCreateInfo  depthStencilInfo = VkInit::Pipeline_DepthStencilCreateInfo();

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
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.SetDefaultDynamicStates();

    Vk_GraphicsPipelineBuildInfo diag{};
    diag.myLabel          = "basic-lit-opaque-hybrid-resolve";
    diag.myVertShaderPath = vertShaderPath.c_str();
    diag.myFragShaderPath = fragShaderPath.c_str();
    diag.myColorFormat    = aCore.mySwapchainCtx.mySwapChainImageFormat;
    diag.myDepthFormat    = aCore.FindDepthFormat();

    pipelineBuilder.myPipelineLayout = aCore.mySceneGpuCtx.myPipelineLayout;
    aCore.mySceneGpuCtx.myBasicPipelineHybridResolve =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myHybridResolveRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );

    pipelineBuilder.myDepthStencil         = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
    pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachmentAlpha();
    diag.myLabel                           = "basic-lit-transparent-hybrid-resolve";
    aCore.mySceneGpuCtx.myTransparentPipelineHybridResolve =
        pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myHybridResolveRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );

    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        VkShaderModule bindlessFragModule = aCore.CreateShaderModule( bindlessFragShaderPath );
        pipelineBuilder.myShaderStages[ 1 ] = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, bindlessFragModule, "main" );
        pipelineBuilder.myPipelineLayout    = aCore.mySceneGpuCtx.myBindlessPipelineLayout;
        pipelineBuilder.myDepthStencil      = depthStencilInfo;
        pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
        diag.myLabel                        = "basic-lit-bindless-opaque-hybrid-resolve";
        diag.myFragShaderPath               = bindlessFragShaderPath.c_str();
        aCore.mySceneGpuCtx.myBasicPipelineBindlessHybridResolve =
            pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myHybridResolveRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );

        pipelineBuilder.myDepthStencil         = VkInit::Pipeline_DepthStencilCreateInfo( VK_FALSE );
        pipelineBuilder.myColorBlendAttachment = VkInit::Pipeline_ColorBlendAttachmentAlpha();
        diag.myLabel                           = "basic-lit-bindless-transparent-hybrid-resolve";
        aCore.mySceneGpuCtx.myTransparentPipelineBindlessHybridResolve =
            pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aCore.mySwapchainCtx.myHybridResolveRenderPass, aCore.myDeviceCtx.myPipelineCache, &diag );
        vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, bindlessFragModule, nullptr );
    }

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragShaderModule, nullptr );
}

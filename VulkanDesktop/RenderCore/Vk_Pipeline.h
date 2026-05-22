#pragma once
#include <vector>
#include <vulkan/vulkan.h>

struct Vk_GraphicsPipelineBuildInfo;

class Vk_PipelineBuilder {
public:
    VkPipelineVertexInputStateCreateInfo   myVertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo myInputAssembly;
    VkViewport                             myViewport;
    VkRect2D                               myScissor;
    VkPipelineRasterizationStateCreateInfo myRasterizer;
    VkPipelineColorBlendAttachmentState    myColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo   myMultisampling;
    VkPipelineLayout                       myPipelineLayout;
    VkPipelineDepthStencilStateCreateInfo  myDepthStencil;
    VkPipelineDynamicStateCreateInfo       myDynamicState;

    std::vector< VkPipelineShaderStageCreateInfo > myShaderStages;

    VkPipeline BuildPipeline( VkDevice aDevice, VkRenderPass aPass, const Vk_GraphicsPipelineBuildInfo* aDiagnostics = nullptr );
};
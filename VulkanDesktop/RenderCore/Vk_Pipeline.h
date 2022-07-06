#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class PipelineBuilder {
	public:
        VkPipelineVertexInputStateCreateInfo           myVertexInputInfo;
        VkPipelineInputAssemblyStateCreateInfo         myInputAssembly;
        VkViewport                                     myViewport;
        VkRect2D                                       myScissor;
        VkPipelineRasterizationStateCreateInfo         myRasterizer;
        VkPipelineColorBlendAttachmentState            myColorBlendAttachment;
        VkPipelineMultisampleStateCreateInfo           myMultisampling;
        VkPipelineLayout                               myPipelineLayout;
        VkPipelineDepthStencilStateCreateInfo          myDepthStencil;

        std::vector< VkPipelineShaderStageCreateInfo > myShaderStages;

        VkPipeline BuildPipeline( VkDevice aDevice, VkRenderPass aPass );
};
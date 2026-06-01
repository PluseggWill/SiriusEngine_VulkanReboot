#pragma once
#include <initializer_list>
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
    std::vector< VkDynamicState >                myDynamicStatesStorage;

    // Copies aStates into owned storage; wires myDynamicState.pDynamicStates for pipeline create.
    void SetDynamicStates( std::initializer_list< VkDynamicState > aStates );
    // Scene default: viewport + scissor + line width (see Vk_Core::SetGraphicsDynamicState).
    void SetDefaultDynamicStates();

    // aPipelineCache: Vk_Core::myPipelineCache (VK_NULL_HANDLE allowed but disables cross-run reuse).
    VkPipeline BuildPipeline( VkDevice aDevice, VkRenderPass aPass, VkPipelineCache aPipelineCache,
                              const Vk_GraphicsPipelineBuildInfo* aDiagnostics = nullptr );
};
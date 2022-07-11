#include "Vk_Pipeline.h"

#include <stdexcept>

VkPipeline PipelineBuilder::BuildPipeline( VkDevice aDevice, VkRenderPass aPass ) {
    // Create the viewport state
    VkPipelineViewportStateCreateInfo viewportState{};

    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &myViewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &myScissor;

    // Setup dummy color blending. The blending is just "no blend" for there's no transparent objects yet
    VkPipelineColorBlendStateCreateInfo colorBlending{};

    colorBlending.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable       = VK_FALSE;
    colorBlending.logicOp             = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount     = 1;
    colorBlending.pAttachments        = &myColorBlendAttachment;
    colorBlending.blendConstants[ 0 ] = 0.0f;
    colorBlending.blendConstants[ 1 ] = 0.0f;
    colorBlending.blendConstants[ 2 ] = 0.0f;
    colorBlending.blendConstants[ 3 ] = 0.0f;

    // Build the actual pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};

    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast< uint32_t >( myShaderStages.size() );
    pipelineInfo.pStages             = myShaderStages.data();
    pipelineInfo.pVertexInputState   = &myVertexInputInfo;
    pipelineInfo.pInputAssemblyState = &myInputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &myRasterizer;
    pipelineInfo.pMultisampleState   = &myMultisampling;
    pipelineInfo.pDepthStencilState  = &myDepthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = nullptr;
    pipelineInfo.layout              = myPipelineLayout;
    pipelineInfo.renderPass          = aPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    VkPipeline newPipeline;
    if ( vkCreateGraphicsPipelines( aDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create render pass!" );
    }

    return newPipeline;
}

#include "Vk_Pipeline.h"
#include "Vk_Initializer.h"
#include "Vk_PipelineDiagnostics.h"

#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include <stdexcept>
#include <string>

namespace {
const std::initializer_list< VkDynamicState > kDefaultGraphicsDynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_LINE_WIDTH,
};
}  // namespace

void Vk_PipelineBuilder::SetDynamicStates( std::initializer_list< VkDynamicState > aStates ) {
    myDynamicStatesStorage.assign( aStates.begin(), aStates.end() );
    VkInit::Pipeline_FillDynamicStateCreateInfo( myDynamicStatesStorage, myDynamicState );
}

void Vk_PipelineBuilder::SetDefaultDynamicStates() {
    SetDynamicStates( kDefaultGraphicsDynamicStates );
}

VkPipeline Vk_PipelineBuilder::BuildPipeline( VkDevice aDevice, VkRenderPass aPass, const Vk_GraphicsPipelineBuildInfo* aDiagnostics ) {
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
    pipelineInfo.pDynamicState = ( myDynamicState.dynamicStateCount > 0 ) ? &myDynamicState : nullptr;
    pipelineInfo.layout              = myPipelineLayout;
    pipelineInfo.renderPass          = aPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    if ( aDiagnostics != nullptr ) {
        VkPipelineDiagnostics::LogGraphicsPipelineSummary( *aDiagnostics, *this, aPass );
    }

    VkPipeline newPipeline;
    const VkResult createResult = vkCreateGraphicsPipelines( aDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline );
    if ( createResult != VK_SUCCESS ) {
        if ( aDiagnostics != nullptr ) {
            UtilLogger::Error( "PIPELINE", "vkCreateGraphicsPipelines failed after summary for: " + std::string( aDiagnostics->myLabel ) );
            VkPipelineDiagnostics::LogGraphicsPipelineSummary( *aDiagnostics, *this, aPass );
        }
        UtilVulkanResult::ThrowOnFailure( createResult, "vkCreateGraphicsPipelines" );
    }

    if ( aDiagnostics != nullptr ) {
        UtilLogger::Info( "PIPELINE", std::string( aDiagnostics->myLabel ) + " graphics pipeline created (VkResult=VK_SUCCESS)." );
    }

    return newPipeline;
}

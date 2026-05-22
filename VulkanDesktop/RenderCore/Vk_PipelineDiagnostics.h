#pragma once

#include "Vk_Pipeline.h"

#include <vulkan/vulkan.h>

// Optional context for logging graphics pipeline create summary ([PIPELINE]).
struct Vk_GraphicsPipelineBuildInfo {
    const char*         myLabel                    = "graphics";
    const char*         myVertShaderPath           = nullptr;
    const char*         myFragShaderPath           = nullptr;
    uint32_t            myPipelineLayoutSetCount   = 0;
    uint32_t            myPipelineLayoutPushCount  = 0;
    VkFormat            myColorFormat              = VK_FORMAT_UNDEFINED;
    VkFormat            myDepthFormat              = VK_FORMAT_UNDEFINED;
};

namespace VkPipelineDiagnostics {
void LogGraphicsPipelineSummary( const Vk_GraphicsPipelineBuildInfo& aInfo, const Vk_PipelineBuilder& aBuilder, VkRenderPass aRenderPass );
}  // namespace VkPipelineDiagnostics

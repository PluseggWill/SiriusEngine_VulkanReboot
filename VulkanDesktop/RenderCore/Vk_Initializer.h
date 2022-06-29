// Vulkan Initialization helper functions

#pragma once
#include <vector>
#include <vulkan/vulkan.h>

namespace VkInit {
VkPipelineShaderStageCreateInfo Pipeline_VertStageCreateInfo( VkShaderModule aVertShaderModule );

VkPipelineShaderStageCreateInfo Pipeline_FragStageCreateInfo( VkShaderModule aFragShaderModule );

VkPipelineRasterizationStateCreateInfo Pipeline_RasterizationCreateInfo();

VkPipelineMultisampleStateCreateInfo Pipeline_MultisampleCreateInfo( VkSampleCountFlagBits aSampleCount );

VkPipelineDepthStencilStateCreateInfo Pipeline_DepthStencilCreateInfo();

VkPipelineColorBlendAttachmentState Pipeline_ColorBlendAttachment( VkBool32 aBlendEnable );

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( VkPipelineColorBlendAttachmentState anAttachment );

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( std::vector< VkPipelineColorBlendAttachmentState >& someAttachments );

VkViewport ViewportCreateInfo( VkExtent2D aSwapchainExtent );

VkCommandPoolCreateInfo CommandPoolCreateInfo( uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags someFlags = 0 );

VkCommandBufferAllocateInfo CommandBufferAllocInfo( VkCommandPool aPool, uint32_t aCount = 1, VkCommandBufferLevel aBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY );

VkCommandBufferBeginInfo CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags = 0 );

}  // namespace VkInit

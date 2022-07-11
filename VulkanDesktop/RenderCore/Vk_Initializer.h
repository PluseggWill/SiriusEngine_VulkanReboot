// Vulkan Initialization helper functions

#pragma once
#include <vector>
#include <vulkan/vulkan.h>

namespace VkInit {
VkPipelineShaderStageCreateInfo Pipeline_ShaderStageCreateInfo( VkShaderStageFlagBits aStageFlag, VkShaderModule aShaderModule );

VkPipelineVertexInputStateCreateInfo Pipeline_VertexInputStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo Pipeline_InputAssemblyCreateInfo( VkPrimitiveTopology aTopology );

VkPipelineRasterizationStateCreateInfo Pipeline_RasterizationCreateInfo( VkPolygonMode aPolyMode = VK_POLYGON_MODE_FILL, VkCullModeFlags aCullMode = VK_CULL_MODE_BACK_BIT,
                                                                         VkFrontFace aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE );

VkPipelineMultisampleStateCreateInfo Pipeline_MultisampleCreateInfo( VkSampleCountFlagBits aSampleCount );

VkPipelineDynamicStateCreateInfo Pipeline_DynamicStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo Pipeline_DepthStencilCreateInfo();

VkPipelineLayoutCreateInfo Pipeline_LayoutCreateInfo();

VkPipelineColorBlendAttachmentState Pipeline_ColorBlendAttachment( VkBool32 aBlendEnable );

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( VkPipelineColorBlendAttachmentState anAttachment );

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( std::vector< VkPipelineColorBlendAttachmentState >& someAttachments );

VkViewport ViewportCreateInfo( VkExtent2D aSwapchainExtent );

VkCommandPoolCreateInfo CommandPoolCreateInfo( uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags someFlags = 0 );

VkCommandBufferAllocateInfo CommandBufferAllocInfo( VkCommandPool aPool, uint32_t aCount = 1, VkCommandBufferLevel aBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY );

VkCommandBufferBeginInfo CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags = 0 );

}  // namespace VkInit

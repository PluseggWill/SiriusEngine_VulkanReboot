// Module: Vk_Initializer — factory helpers for Vulkan *CreateInfo / write structs used at init time.

#pragma once
#include <array>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace VkInit {

// --- Graphics pipeline (vkCreateGraphicsPipelines) ---

VkPipelineShaderStageCreateInfo Pipeline_ShaderStageCreateInfo( VkShaderStageFlagBits aStageFlag, VkShaderModule aShaderModule, const char* anEntry );

// HLSL/dxc entry names (VSMain / PSMain). Active GLSL path uses Pipeline_ShaderStageCreateInfo(..., "main") instead.
VkPipelineShaderStageCreateInfo Pipeline_VertexShaderStageCreateInfo( VkShaderModule aShaderModule );
VkPipelineShaderStageCreateInfo Pipeline_PixelShaderStageCreateInfo( VkShaderModule aShaderModule );

VkPipelineVertexInputStateCreateInfo Pipeline_VertexInputStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo Pipeline_InputAssemblyCreateInfo( VkPrimitiveTopology aTopology );

VkPipelineRasterizationStateCreateInfo Pipeline_RasterizationCreateInfo( VkPolygonMode aPolyMode = VK_POLYGON_MODE_FILL, VkCullModeFlags aCullMode = VK_CULL_MODE_BACK_BIT,
                                                                         VkFrontFace aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE );

VkPipelineMultisampleStateCreateInfo Pipeline_MultisampleCreateInfo( VkSampleCountFlagBits aSampleCount );

// Preferred: caller-owned vector + Fill. Legacy: default viewport/line-width list (static storage).
void                              Pipeline_FillDynamicStateCreateInfo( const std::vector< VkDynamicState >& aStorage, VkPipelineDynamicStateCreateInfo& aOut );
VkPipelineDynamicStateCreateInfo  Pipeline_DynamicStateCreateInfo();

VkPipelineDepthStencilStateCreateInfo Pipeline_DepthStencilCreateInfo( VkBool32 aDepthWriteEnable = VK_TRUE );

VkPipelineLayoutCreateInfo Pipeline_LayoutCreateInfo();

VkPipelineColorBlendAttachmentState Pipeline_ColorBlendAttachment( VkBool32 aBlendEnable );
VkPipelineColorBlendAttachmentState Pipeline_ColorBlendAttachmentAlpha();

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( VkPipelineColorBlendAttachmentState anAttachment );

VkPipelineColorBlendStateCreateInfo Pipeline_ColorBlendCreateInfo( std::vector< VkPipelineColorBlendAttachmentState >& someAttachments );

// --- Viewport (embedded in VkPipelineViewportStateCreateInfo in Vk_PipelineBuilder) ---

VkViewport ViewportCreateInfo( VkExtent2D aSwapchainExtent );

// --- Commands (vkCreateCommandPool / vkAllocateCommandBuffers / vkBeginCommandBuffer) ---

VkCommandPoolCreateInfo CommandPoolCreateInfo( uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags someFlags = 0 );

VkCommandBufferAllocateInfo CommandBufferAllocInfo( VkCommandPool aPool, uint32_t aCount = 1, VkCommandBufferLevel aBufferLevel = VK_COMMAND_BUFFER_LEVEL_PRIMARY );

VkCommandBufferBeginInfo CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags = 0 );

// --- Images (vkCreateImage / vkCreateImageView) ---

VkImageCreateInfo ImageCreateInfo( VkFormat aFormat, VkImageUsageFlags aUsage, VkExtent3D anExtent );
void              FillImageSharingMode( uint32_t aGraphicsQueueFamily, uint32_t aTransferQueueFamily, std::array< uint32_t, 2 >& someQueueFamilyIndices,
                                        VkImageCreateInfo& aInOut );

VkImageViewCreateInfo ImageViewCreateInfo( VkFormat aFormat, VkImage anImage, VkImageAspectFlags anAspect, uint32_t aMipLevel );

// --- Descriptors (vkCreateDescriptorSetLayout / vkUpdateDescriptorSets) ---

VkWriteDescriptorSet DescriptorSetWriteCreateInfo( VkDescriptorSet aDstSet, VkDescriptorType aType, VkDescriptorImageInfo* aImageInfo, uint32_t aBinding, uint32_t aCount );

VkWriteDescriptorSet DescriptorSetWriteCreateInfo( VkDescriptorSet aDstSet, VkDescriptorType aType, VkDescriptorBufferInfo* aBufferInfo, uint32_t aBinding, uint32_t aCount );

VkDescriptorSetLayoutBinding DescriptorSetLayoutBindingCreateInfo( VkDescriptorType aType, VkShaderStageFlags someStageFlags, uint32_t aBinding );

}  // namespace VkInit

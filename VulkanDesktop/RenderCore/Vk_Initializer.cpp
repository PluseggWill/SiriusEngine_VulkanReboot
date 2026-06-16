// Module: Vk_Initializer — default Vulkan create-info helpers for Vk_Renderer and Vk_PipelineBuilder.

#include "Vk_Initializer.h"

#include <array>
#include <vector>

// --- Graphics pipeline stages ---

// VkPipelineShaderStageCreateInfo — one shader stage for vkCreateGraphicsPipelines (pStages[]).
// Used when: assembling Vk_PipelineBuilder::myShaderStages before BuildPipeline.
// Example (GLSL): Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) in Vk_Renderer::CreateGfxPipeline.
VkPipelineShaderStageCreateInfo VkInit::Pipeline_ShaderStageCreateInfo( VkShaderStageFlagBits aStageFlag, VkShaderModule aShaderModule, const char* anEntry ) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};

    shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage  = aStageFlag;
    shaderStageInfo.module = aShaderModule;
    shaderStageInfo.pName  = anEntry;

    return shaderStageInfo;
}

// VkPipelineShaderStageCreateInfo — vertex stage preset for HLSL/dxc (entry VSMain).
// Used when: legacy or future dxc path; not used by current glslc shaders (entry "main").
// Example: Pipeline_VertexShaderStageCreateInfo( vertModule ) with TriangleShader.hlsl + dxc.
VkPipelineShaderStageCreateInfo VkInit::Pipeline_VertexShaderStageCreateInfo( VkShaderModule aShaderModule ) {
    return Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, aShaderModule, "VSMain" );
}

// VkPipelineShaderStageCreateInfo — fragment stage preset for HLSL/dxc (entry PSMain).
// Used when: legacy or future dxc path; not used by current glslc shaders (entry "main").
// Example: Pipeline_PixelShaderStageCreateInfo( fragModule ) with TriangleShader.hlsl + dxc.
VkPipelineShaderStageCreateInfo VkInit::Pipeline_PixelShaderStageCreateInfo( VkShaderModule aShaderModule ) {
    return Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, aShaderModule, "PSMain" );
}

// VkPipelineVertexInputStateCreateInfo — vertex bindings/attributes (pVertexInputState); often overridden after call.
// Used when: CreateGfxPipeline seeds layout, then sets pVertexBindingDescriptions from Gfx_Vertex.
// Example: vertexInputInfo = Pipeline_VertexInputStateCreateInfo(); then fill bindingDescription from Gfx_Vertex::getBindingDescription().
VkPipelineVertexInputStateCreateInfo VkInit::Pipeline_VertexInputStateCreateInfo() {
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    return vertexInputInfo;
}

// VkPipelineDynamicStateCreateInfo — dynamic state list (pDynamicState); pDynamicStates must outlive vkCreateGraphicsPipelines.
// Used when: Vk_PipelineBuilder::SetDynamicStates copies enums then calls this Fill on owned storage.
// Example: vector states = { VK_DYNAMIC_STATE_VIEWPORT }; Pipeline_FillDynamicStateCreateInfo( states, builder.myDynamicState ).
void VkInit::Pipeline_FillDynamicStateCreateInfo( const std::vector< VkDynamicState >& aStorage, VkPipelineDynamicStateCreateInfo& aOut ) {
    aOut                   = {};
    aOut.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    aOut.dynamicStateCount = static_cast< uint32_t >( aStorage.size() );
    aOut.pDynamicStates    = aStorage.empty() ? nullptr : aStorage.data();
}

// VkPipelineInputAssemblyStateCreateInfo — primitive topology (pInputAssemblyState).
// Used when: CreateGfxPipeline for indexed triangle meshes.
// Example: Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ).
VkPipelineInputAssemblyStateCreateInfo VkInit::Pipeline_InputAssemblyCreateInfo( VkPrimitiveTopology aTopology ) {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};

    inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology               = aTopology;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    return inputAssemblyInfo;
}

// VkViewport — single viewport (pViewports in VkPipelineViewportStateCreateInfo); not a standalone create call.
// Used when: Vk_PipelineBuilder::myViewport from swapchain extent in CreateGfxPipeline.
// Example: pipelineBuilder.myViewport = ViewportCreateInfo( mySwapChainExtent ).
VkViewport VkInit::ViewportCreateInfo( VkExtent2D aSwapchainExtent ) {
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = ( float )aSwapchainExtent.width;
    viewport.height   = ( float )aSwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    return viewport;
}

// VkPipelineRasterizationStateCreateInfo — fill mode, cull, front face (pRasterizationState).
// Used when: CreateGfxPipeline; FILL_MODE_LINE toggles VK_POLYGON_MODE_LINE for debug wireframe.
// Example: Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL ) with back-face cull defaults.
VkPipelineRasterizationStateCreateInfo VkInit::Pipeline_RasterizationCreateInfo( VkPolygonMode   aPolyMode /*= VK_POLYGON_MODE_FILL*/,
                                                                                 VkCullModeFlags aCullMode /*= VK_CULL_MODE_BACK_BIT*/,
                                                                                 VkFrontFace     aFrontFace /*= VK_FRONT_FACE_COUNTER_CLOCKWISE*/ ) {
    VkPipelineRasterizationStateCreateInfo rasterizer{};

    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = aPolyMode;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = aCullMode;
    rasterizer.frontFace               = aFrontFace;
    rasterizer.depthBiasEnable         = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp          = 0.0f;
    rasterizer.depthBiasSlopeFactor    = 0.0f;

    return rasterizer;
}

// VkPipelineMultisampleStateCreateInfo — MSAA sample count (pMultisampleState).
// Used when: CreateGfxPipeline; myMSAASamples is VK_SAMPLE_COUNT_1_BIT on current stable path.
// Example: Pipeline_MultisampleCreateInfo( myMSAASamples ) before BuildPipeline.
VkPipelineMultisampleStateCreateInfo VkInit::Pipeline_MultisampleCreateInfo( VkSampleCountFlagBits aSampleCount ) {
    VkPipelineMultisampleStateCreateInfo multisampling{};

    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_TRUE;
    multisampling.rasterizationSamples  = aSampleCount;
    multisampling.minSampleShading      = .2f;
    multisampling.pSampleMask           = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable      = VK_FALSE;

    return multisampling;
}

// VkPipelineDepthStencilStateCreateInfo — depth test/write (pDepthStencilState).
// Used when: CreateGfxPipeline with depth attachment in render pass.
// Example: Pipeline_DepthStencilCreateInfo() — LESS compare; transparent pass uses depthWriteEnable=VK_FALSE.
VkPipelineDepthStencilStateCreateInfo VkInit::Pipeline_DepthStencilCreateInfo( VkBool32 aDepthWriteEnable ) {
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};

    depthStencilInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable       = VK_TRUE;
    depthStencilInfo.depthWriteEnable      = aDepthWriteEnable;
    depthStencilInfo.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds        = 0.0f;
    depthStencilInfo.maxDepthBounds        = 1.0f;
    depthStencilInfo.stencilTestEnable     = VK_FALSE;
    depthStencilInfo.front                 = {};
    depthStencilInfo.back                  = {};

    return depthStencilInfo;
}

// VkPipelineLayoutCreateInfo — descriptor set layouts + push constants for vkCreatePipelineLayout.
// Used when: CreateGfxPipeline; caller sets setLayoutCount / pSetLayouts after this template.
// Example: auto layoutInfo = Pipeline_LayoutCreateInfo(); layoutInfo.setLayoutCount = 1; layoutInfo.pSetLayouts = &myGlobalSetLayout.
VkPipelineLayoutCreateInfo VkInit::Pipeline_LayoutCreateInfo() {
    VkPipelineLayoutCreateInfo layoutInfo{};

    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.flags          = 0;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts    = nullptr;
    // Push constant ranges are set by the caller (e.g. Vk_Renderer::CreateGfxPipeline — mat4 model, VERTEX).
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges    = nullptr;

    return layoutInfo;
}

// VkPipelineColorBlendAttachmentState — one render-target blend state (element of pAttachments).
// Used when: building color blend state for swapchain/MSAA color attachment.
// Example: Pipeline_ColorBlendAttachment( VK_FALSE ) — opaque pass-through, no blending.
VkPipelineColorBlendAttachmentState VkInit::Pipeline_ColorBlendAttachment( VkBool32 aBlendEnable ) {
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = aBlendEnable;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    return colorBlendAttachment;
}

VkPipelineColorBlendAttachmentState VkInit::Pipeline_ColorBlendAttachmentAlpha() {
    VkPipelineColorBlendAttachmentState attachment = Pipeline_ColorBlendAttachment( VK_TRUE );
    attachment.srcColorBlendFactor                 = VK_BLEND_FACTOR_SRC_ALPHA;
    attachment.dstColorBlendFactor                 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attachment.srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE;
    attachment.dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    return attachment;
}

// VkPipelineColorBlendStateCreateInfo — full blend state (pColorBlendState); pAttachments must outlive create.
// Used when: multiple color attachments (MSAA resolve / MRT); vector must stay alive through vkCreateGraphicsPipelines.
// Example: vector attachments = { Pipeline_ColorBlendAttachment( VK_FALSE ) }; Pipeline_ColorBlendCreateInfo( attachments ).
VkPipelineColorBlendStateCreateInfo VkInit::Pipeline_ColorBlendCreateInfo( std::vector< VkPipelineColorBlendAttachmentState >& someAttachments ) {
    VkPipelineColorBlendStateCreateInfo colorBlending{};

    colorBlending.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable       = VK_FALSE;
    colorBlending.logicOp             = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount     = static_cast< uint32_t >( someAttachments.size() );
    colorBlending.pAttachments        = someAttachments.data();
    colorBlending.blendConstants[ 0 ] = 0.0f;
    colorBlending.blendConstants[ 1 ] = 0.0f;
    colorBlending.blendConstants[ 2 ] = 0.0f;
    colorBlending.blendConstants[ 3 ] = 0.0f;

    return colorBlending;
}

// VkPipelineColorBlendStateCreateInfo — single attachment via pointer to stack/local attachment (pAttachments = &anAttachment).
// Used when: Vk_PipelineBuilder embeds one VkPipelineColorBlendAttachmentState on the builder.
// Example: colorBlendAttachment = Pipeline_ColorBlendAttachment( VK_FALSE ); used as builder.myColorBlendAttachment.
VkPipelineColorBlendStateCreateInfo VkInit::Pipeline_ColorBlendCreateInfo( VkPipelineColorBlendAttachmentState anAttachment ) {
    VkPipelineColorBlendStateCreateInfo colorBlending{};

    colorBlending.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable       = VK_FALSE;
    colorBlending.logicOp             = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount     = 1;
    colorBlending.pAttachments        = &anAttachment;
    colorBlending.blendConstants[ 0 ] = 0.0f;
    colorBlending.blendConstants[ 1 ] = 0.0f;
    colorBlending.blendConstants[ 2 ] = 0.0f;
    colorBlending.blendConstants[ 3 ] = 0.0f;

    return colorBlending;
}

// --- Command buffers ---

// VkCommandPoolCreateInfo — vkCreateCommandPool for a queue family.
// Used when: Vk_Renderer::CreateCommandPool for graphics and transfer pools.
// Example: CommandPoolCreateInfo( graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT ).
VkCommandPoolCreateInfo VkInit::CommandPoolCreateInfo( uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags someFlags /*= 0*/ ) {
    VkCommandPoolCreateInfo poolInfo{};

    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = someFlags;
    poolInfo.queueFamilyIndex = aQueueFamilyIndex;

    return poolInfo;
}

// VkCommandBufferAllocateInfo — vkAllocateCommandBuffers from a pool.
// Used when: per-frame primary buffers in CreateFrameData; one-shot transfer in CopyBufferToImage path.
// Example: CommandBufferAllocInfo( myGraphicsCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY ).
VkCommandBufferAllocateInfo VkInit::CommandBufferAllocInfo( VkCommandPool aPool, uint32_t aCount /*= 1*/,
                                                            VkCommandBufferLevel aBufferLevel /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/ ) {
    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext              = nullptr;
    allocInfo.level              = aBufferLevel;
    allocInfo.commandPool        = aPool;
    allocInfo.commandBufferCount = aCount;

    return allocInfo;
}

// VkCommandBufferBeginInfo — vkBeginCommandBuffer.
// Used when: recording draw/upload commands each frame or one-shot uploads.
// Example: CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT ) for staging copy.
VkCommandBufferBeginInfo VkInit::CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags /*=0*/ ) {
    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = someFlags;
    beginInfo.pInheritanceInfo = nullptr;

    return beginInfo;
}

// --- Images ---

// VkImageCreateInfo — vkCreateImage template (2D, optimal tiling, exclusive sharing by default).
// Used when: Vk_Renderer::CreateImage before VMA allocation; caller may override samples/sharing (e.g. FillImageSharingMode for graphics+transfer).
// Example: ImageCreateInfo( VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, extent ) for textures.
VkImageCreateInfo VkInit::ImageCreateInfo( VkFormat aFormat, VkImageUsageFlags aUsage, VkExtent3D anExtent ) {
    VkImageCreateInfo imageInfo{};

    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = anExtent;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = aFormat;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = aUsage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags         = 0;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    return imageInfo;
}

void VkInit::FillImageSharingMode( uint32_t aGraphicsQueueFamily, uint32_t aTransferQueueFamily, std::array< uint32_t, 2 >& someQueueFamilyIndices,
                                   VkImageCreateInfo& aInOut ) {
    // Same-family fallback must stay EXCLUSIVE; CONCURRENT with duplicate family indices is invalid.
    if ( aGraphicsQueueFamily == aTransferQueueFamily ) {
        aInOut.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        aInOut.queueFamilyIndexCount = 0;
        aInOut.pQueueFamilyIndices   = nullptr;
        return;
    }

    someQueueFamilyIndices       = { aGraphicsQueueFamily, aTransferQueueFamily };
    aInOut.sharingMode           = VK_SHARING_MODE_CONCURRENT;
    aInOut.queueFamilyIndexCount = 2;
    aInOut.pQueueFamilyIndices   = someQueueFamilyIndices.data();
}

// VkImageViewCreateInfo — vkCreateImageView for a 2D subresource range.
// Used when: CreateImageView for color/depth textures after image creation.
// Example: ImageViewCreateInfo( depthFormat, depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, 1 ).
VkImageViewCreateInfo VkInit::ImageViewCreateInfo( VkFormat aFormat, VkImage anImage, VkImageAspectFlags anAspect, uint32_t aMipLevel ) {
    VkImageViewCreateInfo viewInfo{};

    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = anImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = aFormat;
    viewInfo.subresourceRange.aspectMask     = anAspect;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = aMipLevel;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    return viewInfo;
}

// --- Descriptors ---

// VkWriteDescriptorSet — vkUpdateDescriptorSets for combined image sampler.
// Used when: binding albedo texture to Set 1 binding eVk_MaterialTextureBinding per material.
// Example: DescriptorSetWriteCreateInfo( set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, eVk_MaterialTextureBinding, 1 ).
VkWriteDescriptorSet VkInit::DescriptorSetWriteCreateInfo( VkDescriptorSet aDstSet, VkDescriptorType aType, VkDescriptorImageInfo* aImageInfo, uint32_t aBinding,
                                                           uint32_t aCount ) {
    VkWriteDescriptorSet descriptorWrite{};

    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet           = aDstSet;
    descriptorWrite.dstBinding       = aBinding;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorType   = aType;
    descriptorWrite.descriptorCount  = aCount;
    descriptorWrite.pBufferInfo      = nullptr;
    descriptorWrite.pImageInfo       = aImageInfo;
    descriptorWrite.pTexelBufferView = nullptr;
    descriptorWrite.pNext            = nullptr;

    return descriptorWrite;
}

// VkWriteDescriptorSet — vkUpdateDescriptorSets for uniform buffer(s).
// Used when: camera UBO (binding 0) and environment UBO (binding 1) on Set 0 each frame.
// Example: DescriptorSetWriteCreateInfo( set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &camBufferInfo, eVk_CameraBinding, 1 ).
VkWriteDescriptorSet VkInit::DescriptorSetWriteCreateInfo( VkDescriptorSet aDstSet, VkDescriptorType aType, VkDescriptorBufferInfo* aBufferInfo, uint32_t aBinding,
                                                           uint32_t aCount ) {
    VkWriteDescriptorSet descriptorWrite{};

    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet           = aDstSet;
    descriptorWrite.dstBinding       = aBinding;
    descriptorWrite.dstArrayElement  = 0;
    descriptorWrite.descriptorType   = aType;
    descriptorWrite.descriptorCount  = aCount;
    descriptorWrite.pBufferInfo      = aBufferInfo;
    descriptorWrite.pImageInfo       = nullptr;
    descriptorWrite.pTexelBufferView = nullptr;
    descriptorWrite.pNext            = nullptr;

    return descriptorWrite;
}

// VkDescriptorSetLayoutBinding — one binding in vkCreateDescriptorSetLayout.
// Used when: CreateDescriptorSetLayout for camera, env, and texture slots (eVk_* in Vk_Enum.h).
// Example: DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, eVk_CameraBinding ).
VkDescriptorSetLayoutBinding VkInit::DescriptorSetLayoutBindingCreateInfo( VkDescriptorType aType, VkShaderStageFlags someStageFlags, uint32_t aBinding ) {
    VkDescriptorSetLayoutBinding setBind{};

    setBind.binding            = aBinding;
    setBind.descriptorCount    = 1;
    setBind.descriptorType     = aType;
    setBind.pImmutableSamplers = nullptr;
    setBind.stageFlags         = someStageFlags;

    return setBind;
}

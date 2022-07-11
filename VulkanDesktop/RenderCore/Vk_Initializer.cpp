#include "Vk_Initializer.h"

VkPipelineShaderStageCreateInfo VkInit::Pipeline_ShaderStageCreateInfo( VkShaderStageFlagBits aStageFlag, VkShaderModule aShaderModule ) {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};

    shaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage  = aStageFlag;
    shaderStageInfo.module = aShaderModule;
    shaderStageInfo.pName  = "main";

    return shaderStageInfo;
}

VkPipelineVertexInputStateCreateInfo VkInit::Pipeline_VertexInputStateCreateInfo() {
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    return vertexInputInfo;
}

VkPipelineDynamicStateCreateInfo VkInit::Pipeline_DynamicStateCreateInfo() {
    std::vector< VkDynamicState > dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast< uint32_t >( dynamicStates.size() );
    dynamicState.pDynamicStates    = dynamicStates.data();

    return dynamicState;
}

VkPipelineInputAssemblyStateCreateInfo VkInit::Pipeline_InputAssemblyCreateInfo( VkPrimitiveTopology aTopology ) {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};

    inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology               = aTopology;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    return inputAssemblyInfo;
}

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

VkPipelineRasterizationStateCreateInfo VkInit::Pipeline_RasterizationCreateInfo( VkPolygonMode aPolyMode /*= VK_POLYGON_MODE_FILL*/, VkCullModeFlags aCullMode /*= VK_CULL_MODE_BACK_BIT*/,
                                                                                 VkFrontFace aFrontFace /*= VK_FRONT_FACE_COUNTER_CLOCKWISE*/ ) {
    VkPipelineRasterizationStateCreateInfo rasterizer{};

    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = aPolyMode;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = aCullMode;
    rasterizer.frontFace               = aFrontFace;
    rasterizer.depthBiasClamp          = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp          = 0.0f;
    rasterizer.depthBiasSlopeFactor    = 0.0f;

    return rasterizer;
}

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

VkPipelineDepthStencilStateCreateInfo VkInit::Pipeline_DepthStencilCreateInfo() {
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};

    depthStencilInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable       = VK_TRUE;
    depthStencilInfo.depthWriteEnable      = VK_TRUE;
    depthStencilInfo.depthCompareOp        = VK_COMPARE_OP_LESS;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds        = 0.0f;
    depthStencilInfo.maxDepthBounds        = 1.0f;
    depthStencilInfo.stencilTestEnable     = VK_FALSE;
    depthStencilInfo.front                 = {};
    depthStencilInfo.back                  = {};

    return depthStencilInfo;
}

VkPipelineLayoutCreateInfo VkInit::Pipeline_LayoutCreateInfo() {
    VkPipelineLayoutCreateInfo layoutInfo{};

    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.flags                  = 0;
    layoutInfo.setLayoutCount         = 0;
    layoutInfo.pSetLayouts            = nullptr;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges    = nullptr;

    return layoutInfo;
}

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

VkCommandPoolCreateInfo VkInit::CommandPoolCreateInfo( uint32_t aQueueFamilyIndex, VkCommandPoolCreateFlags someFlags /*= 0*/ ) {
    VkCommandPoolCreateInfo poolInfo{};

    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = someFlags;
    poolInfo.queueFamilyIndex = aQueueFamilyIndex;

    return poolInfo;
}

VkCommandBufferAllocateInfo VkInit::CommandBufferAllocInfo( VkCommandPool aPool, uint32_t aCount /*= 1*/, VkCommandBufferLevel aBufferLevel /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/ ) {
    VkCommandBufferAllocateInfo allocInfo{};

    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext              = nullptr;
    allocInfo.level              = aBufferLevel;
    allocInfo.commandPool        = aPool;
    allocInfo.commandBufferCount = aCount;

    return allocInfo;
}

VkCommandBufferBeginInfo VkInit::CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags /*=0*/ ) {
    VkCommandBufferBeginInfo beginInfo{};

    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = someFlags;
    beginInfo.pInheritanceInfo = nullptr;

    return beginInfo;
}

#include "Vk_Initializer.h"

VkPipelineShaderStageCreateInfo VkInit::Pipeline_VertStageCreateInfo( VkShaderModule aVertShaderModule ) {
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = aVertShaderModule;
    vertShaderStageInfo.pName  = "main";

    return vertShaderStageInfo;
}

VkPipelineShaderStageCreateInfo VkInit::Pipeline_FragStageCreateInfo( VkShaderModule aFragShaderModule ) {
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = aFragShaderModule;
    fragShaderStageInfo.pName  = "main";

    return fragShaderStageInfo;
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

VkPipelineRasterizationStateCreateInfo VkInit::Pipeline_RasterizationCreateInfo() {
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    allocInfo.level              = aBufferLevel;
    allocInfo.commandPool        = aPool;
    allocInfo.commandBufferCount = aCount;

    return allocInfo;
}

VkCommandBufferBeginInfo VkInit::CommandBufferBeginInfo( VkCommandBufferUsageFlags someFlags /*= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT*/ ) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = someFlags;
    beginInfo.pInheritanceInfo = nullptr;

    return beginInfo;
}

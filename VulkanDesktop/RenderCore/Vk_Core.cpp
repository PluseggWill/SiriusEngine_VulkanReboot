#include "Vk_Core.h"

std::string vertShaderPath = "Shader/TriangleVertex.spv";
std::string fragShaderPath = "Shader/TriangleFrag.spv";

Vk_Core::Vk_Core() {}
Vk_Core::~Vk_Core() {}

Vk_Core& Vk_Core::GetInstance() {
    static Vk_Core myInstance;
    return myInstance;
}

void Vk_Core::SetSize( const uint32_t aWidth, const uint32_t aHeight ) {
    myWidth  = aWidth;
    myHeight = aHeight;
}

void Vk_Core::Reset() {
    myClear();
}

void Vk_Core::Run() {
    myInitWindow();
    myInitVulkan();
    myMainLoop();
    myClear();
}

void Vk_Core::myInitWindow() {
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

    myWindow = glfwCreateWindow( myWidth, myHeight, "Vulkan Window", nullptr, nullptr );
    glfwSetWindowUserPointer( myWindow, this );
    glfwSetFramebufferSizeCallback( myWindow, myFramebufferResizeCallback );
}

void Vk_Core::myMainLoop() {
    while ( !glfwWindowShouldClose( myWindow ) ) {
        glfwPollEvents();
        myDrawFrame();

        // myCurrentFrame = ( myCurrentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle( myDevice );
}

void Vk_Core::myClear() {
    // All other Vulkan resources should be cleaned up
    // before the instance is destroyed.

    myCleanupSwapChain();

    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        vkDestroyBuffer( myDevice, myUniformBuffers[ i ], nullptr );
        vkFreeMemory( myDevice, myUniformBuffersMemory[ i ], nullptr );
    }

    vkDestroyDescriptorPool( myDevice, myDescriptorPool, nullptr );
    vkDestroyDescriptorSetLayout(myDevice, myDescriptorSetLayout, nullptr);

    vkDestroyBuffer( myDevice, myVertexBuffer, nullptr );
    vkFreeMemory( myDevice, myVertexBufferMemory, nullptr );

    vkDestroyBuffer( myDevice, myIndexBuffer, nullptr );
    vkFreeMemory( myDevice, myIndexBufferMemory, nullptr );

    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        vkDestroySemaphore( myDevice, myImageAvailableSemaphores[ i ], nullptr );
        vkDestroySemaphore( myDevice, myRenderFinishedSemaphores[ i ], nullptr );
        vkDestroyFence( myDevice, myInFlightFences[ i ], nullptr );
    }

    vkDestroyCommandPool( myDevice, myGraphicsCommandPool, nullptr );
    vkDestroyCommandPool( myDevice, myTransferCommandPool, nullptr );

    vkDestroyDevice( myDevice, nullptr );

    vkDestroySurfaceKHR( myInstance, mySurface, nullptr );

    vkDestroyInstance( myInstance, nullptr );

    glfwDestroyWindow( myWindow );

    glfwTerminate();
}

void Vk_Core::myInitVulkan() {
    myCreateInstance();
    // TODO: Set up debug messenger
    myCreateSurface();
    myPickPhysicalDevice();
    myCreateLogicalDevice();
    myCreateSwapChain();
    myCreateImageViews();
    myCreateRenderPass();
    myCreateDescriptorSetLayout();
    myCreateGfxPipeline();
    myCreateFrameBuffers();
    myCreateCommandPool();
    myFillVerticesData();
    myCreateVertexBuffer();
    myCreateIndexBuffer();
    myCreateUniformBuffers();
    myCreateDescriptorPool();
    myCreateDescriptorSets();
    myCreateGraphicsCommandBuffers();
    myCreateSyncObjects();
}

void Vk_Core::myCreateInstance() {
    // Check validation layer first
    if ( myEnableValidationLayers && !myCheckValidationLayerSupport() ) {
        throw std::runtime_error( "validation layers requested, but not available!" );
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t     glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

    createInfo.enabledExtensionCount   = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    // Modify the createInfo if the validation layers are enabled
    if ( myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myValidationLayers.data();
    }
    else
        createInfo.enabledLayerCount = 0;

#ifdef _DEBUG
    myCheckExtensionSupport();
#endif  // _DEBUG

    // VkResult result = vkCreateInstance(&createInfo, nullptr, &myInstance);
    if ( vkCreateInstance( &createInfo, nullptr, &myInstance ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create instance!" );
    }
}

void Vk_Core::myPickPhysicalDevice() {
    // The selected GPU will be stored in a VkPhysicalDevice handle
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, nullptr );

    if ( deviceCount == 0 )
        throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

    std::vector< VkPhysicalDevice > devices( deviceCount );
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices ) {
        if ( myCheckDeviceSuitable( device ) ) {
            myPhysicalDevice = device;
            break;
        }
    }

    if ( myPhysicalDevice == VK_NULL_HANDLE )
        throw std::runtime_error( "failed to find a suitable GPU!" );
}

void Vk_Core::myCreateLogicalDevice() {
    // Step #1: Specifying the queues to be created
    QueueFamilyIndices indices = myFindQueueFamilies( myPhysicalDevice );

    std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;
    std::set< uint32_t >                   uniqueQueueFamilies = { indices.myGraphicsFamily.value(), indices.myPresentFamily.value(), indices.myTransferFamily.value() };

    float queuePriority = 1.0f;
    for ( uint32_t queueFamily : uniqueQueueFamilies ) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back( queueCreateInfo );
    }

    // Step #2: Specifying used device features, leave it to be VK_FALSE for now
    VkPhysicalDeviceFeatures deviceFeatures{};

    // Step #3: Creating the logical device (possible to create multiple if needed)
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.queueCreateInfoCount    = static_cast< uint32_t >( queueCreateInfos.size() );
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( myDeviceExtensions.size() );
    createInfo.ppEnabledExtensionNames = myDeviceExtensions.data();

    // Step #4: Enable the validation layers
    if ( myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myValidationLayers.data();
    }
    else
        createInfo.enabledLayerCount = 0;

    // Step #5: Create the logical device
    if ( vkCreateDevice( myPhysicalDevice, &createInfo, nullptr, &myDevice ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create logical device!" );
    }

    // Step #6?: Retrieve queue handles
    vkGetDeviceQueue( myDevice, indices.myGraphicsFamily.value(), 0, &myGraphicsQueue );
    vkGetDeviceQueue( myDevice, indices.myPresentFamily.value(), 0, &myPresentQueue );
    vkGetDeviceQueue( myDevice, indices.myTransferFamily.value(), 0, &myTransferQueue );
}

void Vk_Core::myCreateSurface() {
    if ( glfwCreateWindowSurface( myInstance, myWindow, nullptr, &mySurface ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create window surface!" );
    }
}

void Vk_Core::myCreateSwapChain() {
    SwapChainSupportDetails swapChainSupport = myQuerySwapChainSupport( myPhysicalDevice );

    VkSurfaceFormatKHR surfaceFormat = myChooseSwapSurfaceFormat( swapChainSupport.myFormats );
    VkPresentModeKHR   presentMode   = myChooseSwapPresentMode( swapChainSupport.myPresentModes );
    VkExtent2D         extent        = myChooseSwapExtent( swapChainSupport.myCapabilities );

    // Sometimes have to wait on the driver to complete internal operation.
    // Therefore it is recommended to request at least one more image than the minimum.
    uint32_t imageCount = swapChainSupport.myCapabilities.minImageCount + 1;
    if ( swapChainSupport.myCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.myCapabilities.maxImageCount ) {
        imageCount = swapChainSupport.myCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = mySurface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const QueueFamilyIndices indices              = myFindQueueFamilies( myPhysicalDevice );
    const uint32_t           queueFamilyIndices[] = { indices.myGraphicsFamily.value(), indices.myPresentFamily.value() };

    if ( indices.myGraphicsFamily != indices.myPresentFamily ) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;
    }

    createInfo.preTransform   = swapChainSupport.myCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if ( vkCreateSwapchainKHR( myDevice, &createInfo, nullptr, &mySwapChain ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create swap chain!" );
    }

    // Retrieving the swap chain images
    vkGetSwapchainImagesKHR( myDevice, mySwapChain, &imageCount, nullptr );
    mySwapChainImages.resize( imageCount );
    vkGetSwapchainImagesKHR( myDevice, mySwapChain, &imageCount, mySwapChainImages.data() );

    mySwapChainImageFormat = surfaceFormat.format;
    mySwapChainExtent      = extent;
}

void Vk_Core::myCreateImageViews() {
    mySwapChainImageViews.resize( mySwapChainImages.size() );

    for ( size_t i = 0; i < mySwapChainImages.size(); i++ ) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image                           = mySwapChainImages[ i ];
        createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format                          = mySwapChainImageFormat;
        createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if ( vkCreateImageView( myDevice, &createInfo, nullptr, &mySwapChainImageViews[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create image views!" );
        }
    }
}

void Vk_Core::myCreateGfxPipeline() {
    // Step #1: Load shader code
    auto vertShaderCode = Util_Loader::readFile( vertShaderPath );
    auto fragShaderCode = Util_Loader::readFile( fragShaderPath );

    // Step #2: Create shader module
    VkShaderModule vertShaderModule = myCreateShaderModule( vertShaderCode );
    VkShaderModule fragShaderModule = myCreateShaderModule( fragShaderCode );

    // Step #3: Create shader stage info
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Step #4: Create vertex input state
    auto bindingDescription   = Vertex::getBindingDescription();
    auto attributeDescription = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescription.data();

    // Step #5: Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Step #6: Viewports and Scissors
    // Viewports define the transformation from the image to the framebuffer
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = ( float )mySwapChainExtent.width;
    viewport.height   = ( float )mySwapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissors define the in which regions pixels will actually be stored
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = mySwapChainExtent;

    // Create the viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    // Step #7: Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasClamp          = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp          = 0.0f;
    rasterizer.depthBiasSlopeFactor    = 0.0f;

    // Step #8: Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading      = 1.0f;
    multisampling.pSampleMask           = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable      = VK_FALSE;

    // Step #9: Depth and stencil testing
    // TODO

    // Step #10: Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable       = VK_FALSE;
    colorBlending.logicOp             = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount     = 1;
    colorBlending.pAttachments        = &colorBlendAttachment;
    colorBlending.blendConstants[ 0 ] = 0.0f;
    colorBlending.blendConstants[ 1 ] = 0.0f;
    colorBlending.blendConstants[ 2 ] = 0.0f;
    colorBlending.blendConstants[ 3 ] = 0.0f;

    // Step #11: Dynamic state
    std::vector< VkDynamicState > dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast< uint32_t >( dynamicStates.size() );
    dynamicState.pDynamicStates    = dynamicStates.data();

    // Step #12: Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount         = 1;
    pipelineLayoutCreateInfo.pSetLayouts            = &myDescriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges    = nullptr;

    if ( vkCreatePipelineLayout( myDevice, &pipelineLayoutCreateInfo, nullptr, &myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create pipeline layout!" );
    }

    // Step #13: Combine
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = nullptr;
    pipelineInfo.layout              = myPipelineLayout;
    pipelineInfo.renderPass          = myRenderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    if ( vkCreateGraphicsPipelines( myDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &myGfxPipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphics pipeline!" );
    }

    vkDestroyShaderModule( myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( myDevice, fragShaderModule, nullptr );
}

void Vk_Core::myCreateRenderPass() {
    // Step #1: Attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mySwapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Step #2: Subpasses and attachment references
    VkAttachmentReference colorAttachmentRefs{};
    colorAttachmentRefs.attachment = 0;
    colorAttachmentRefs.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRefs;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Step #3: Render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if ( vkCreateRenderPass( myDevice, &renderPassInfo, nullptr, &myRenderPass ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create render pass!" );
    }
}

void Vk_Core::myCreateFrameBuffers() {
    mySwapChainFrameBuffers.resize( mySwapChainImageViews.size() );

    for ( size_t i = 0; i < mySwapChainImageViews.size(); i++ ) {
        VkImageView attachments[] = { mySwapChainImageViews[ i ] };

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass      = myRenderPass;
        frameBufferInfo.attachmentCount = 1;
        frameBufferInfo.pAttachments    = attachments;
        frameBufferInfo.width           = mySwapChainExtent.width;
        frameBufferInfo.height          = mySwapChainExtent.height;
        frameBufferInfo.layers          = 1;

        if ( vkCreateFramebuffer( myDevice, &frameBufferInfo, nullptr, &mySwapChainFrameBuffers[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create framebuffer!" );
        }
    }
}

void Vk_Core::myCreateCommandPool() {
    QueueFamilyIndices queueFamilyIndices = myFindQueueFamilies( myPhysicalDevice );

    // Graphic queue command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.myGraphicsFamily.value();

    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myGraphicsCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphic command pool!" );
    }

    // Tranfer queue command poo
    poolInfo.queueFamilyIndex = queueFamilyIndices.myTransferFamily.value();
    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myTransferCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create transfer command pool!" );
    }
}

void Vk_Core::myCreateGraphicsCommandBuffers() {
    myGraphicsCommandBuffers.resize( MAX_FRAMES_IN_FLIGHT );
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = myGraphicsCommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = ( uint32_t )myGraphicsCommandBuffers.size();

    if ( vkAllocateCommandBuffers( myDevice, &allocInfo, myGraphicsCommandBuffers.data() ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate command buffers!" );
    }
}

void Vk_Core::myDrawFrame() {
    vkWaitForFences( myDevice, 1, &myInFlightFences[ myCurrentFrame ], VK_TRUE, UINT64_MAX );

    uint32_t imageIndex;
    // Once the image is available for the next frame, myImageAvailableSemaphore will be signaled and ready to be acquired
    VkResult result = vkAcquireNextImageKHR( myDevice, mySwapChain, UINT64_MAX, myImageAvailableSemaphores[ myCurrentFrame ], VK_NULL_HANDLE, &imageIndex );

    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        // Swap chain incompatible, recreate the swap chain
        myRecreateSwapChain();
        return;
    }
    else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
        throw std::runtime_error( "failed to acquire swap chain image!" );
    }

    myUpdateUniformBuffer(myCurrentFrame);

    // Only reset the fence if we are submitting work
    vkResetFences( myDevice, 1, &myInFlightFences[ myCurrentFrame ] );

    vkResetCommandBuffer( myGraphicsCommandBuffers[ myCurrentFrame ], 0 );
    myRecordCommandBuffer( myGraphicsCommandBuffers[ myCurrentFrame ], imageIndex );

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphore[] = { myImageAvailableSemaphores[ myCurrentFrame ] };
    VkPipelineStageFlags waitStages[]    = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount        = 1;
    submitInfo.pWaitSemaphores           = waitSemaphore;
    submitInfo.pWaitDstStageMask         = waitStages;
    submitInfo.commandBufferCount        = 1;
    submitInfo.pCommandBuffers           = &myGraphicsCommandBuffers[ myCurrentFrame ];

    VkSemaphore signalSemaphores[]  = { myRenderFinishedSemaphores[ myCurrentFrame ] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if ( vkQueueSubmit( myGraphicsQueue, 1, &submitInfo, myInFlightFences[ myCurrentFrame ] ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to submit draw command buffer!" );
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = { mySwapChain };
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;
    presentInfo.pImageIndices   = &imageIndex;
    presentInfo.pResults        = nullptr;

    result = vkQueuePresentKHR( myPresentQueue, &presentInfo );
    if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || myFramebufferResized ) {
        myFramebufferResized = false;
        myRecreateSwapChain();
    }
    else if ( result != VK_SUCCESS ) {
        throw std::runtime_error( "failed to present swap chain image!" );
    }

    myCurrentFrame = ( myCurrentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
}

void Vk_Core::myCreateSyncObjects() {
    myImageAvailableSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
    myRenderFinishedSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
    myInFlightFences.resize( MAX_FRAMES_IN_FLIGHT );

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        if ( vkCreateSemaphore( myDevice, &semaphoreInfo, nullptr, &myImageAvailableSemaphores[ i ] ) != VK_SUCCESS
             || vkCreateSemaphore( myDevice, &semaphoreInfo, nullptr, &myRenderFinishedSemaphores[ i ] ) != VK_SUCCESS
             || vkCreateFence( myDevice, &fenceInfo, nullptr, &myInFlightFences[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create semaphores/fence!" );
        }
    }
}

void Vk_Core::myRecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize( myWindow, &width, &height );
    while ( width == 0 || height == 0 ) {
        glfwGetFramebufferSize( myWindow, &width, &height );
        glfwWaitEvents();
    }

    vkDeviceWaitIdle( myDevice );

    myCleanupSwapChain();

    myCreateSwapChain();
    myCreateImageViews();
    myCreateRenderPass();
    myCreateGfxPipeline();
    myCreateFrameBuffers();
}

void Vk_Core::myCleanupSwapChain() {
    for ( size_t i = 0; i < mySwapChainFrameBuffers.size(); i++ ) {
        vkDestroyFramebuffer( myDevice, mySwapChainFrameBuffers[ i ], nullptr );
    }

    vkDestroyPipeline( myDevice, myGfxPipeline, nullptr );
    vkDestroyPipelineLayout( myDevice, myPipelineLayout, nullptr );
    vkDestroyRenderPass( myDevice, myRenderPass, nullptr );

    for ( size_t i = 0; i < mySwapChainImageViews.size(); i++ ) {
        vkDestroyImageView( myDevice, mySwapChainImageViews[ i ], nullptr );
    }

    vkDestroySwapchainKHR( myDevice, mySwapChain, nullptr );
}

void Vk_Core::myFillVerticesData() {
    vertices = { { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                 { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
                 { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
                 { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f } } };

    indices = { 0, 1, 2, 2, 3, 0 };
}

void Vk_Core::myCreateVertexBuffer() {
    const VkDeviceSize bufferSize = sizeof( vertices[ 0 ] ) * vertices.size();

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // The staging buffer memory is allocated at device host, which can be accessed by CPU, but less optimal
    myCreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, true );

    void* data;
    vkMapMemory( myDevice, stagingBufferMemory, 0, bufferSize, 0, &data );
    memcpy( data, vertices.data(), bufferSize );
    vkUnmapMemory( myDevice, stagingBufferMemory );

    // The vertex buffer is, on the other hand, allocated on the device local, which can only be accessed by GPU and offers better performance
    myCreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, myVertexBuffer, myVertexBufferMemory, false );

    myCopyBuffer( stagingBuffer, myVertexBuffer, bufferSize );

    vkDestroyBuffer( myDevice, stagingBuffer, nullptr );
    vkFreeMemory( myDevice, stagingBufferMemory, nullptr );
}

void Vk_Core::myCreateIndexBuffer() {
    const VkDeviceSize bufferSize = sizeof( indices[ 0 ] ) * indices.size();

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // The staging buffer memory is allocated at device host, which can be accessed by CPU, but less optimal
    myCreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory, true );

    void* data;
    vkMapMemory( myDevice, stagingBufferMemory, 0, bufferSize, 0, &data );
    memcpy( data, indices.data(), bufferSize );
    vkUnmapMemory( myDevice, stagingBufferMemory );

    // The vertex buffer is, on the other hand, allocated on the device local, which can only be accessed by GPU and offers better performance
    myCreateBuffer( bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, myIndexBuffer, myIndexBufferMemory, false );

    myCopyBuffer( stagingBuffer, myIndexBuffer, bufferSize );

    vkDestroyBuffer( myDevice, stagingBuffer, nullptr );
    vkFreeMemory( myDevice, stagingBufferMemory, nullptr );
}

void Vk_Core::myCreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(myDevice, &layoutInfo, nullptr, &myDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error( "failed to create descriptor set layout!" );
    }
}

void Vk_Core::myCreateUniformBuffers() {
    VkDeviceSize bufferSize = sizeof( UniformBufferObject );

    myUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    myUniformBuffersMemory.resize( MAX_FRAMES_IN_FLIGHT );

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        myCreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, myUniformBuffers[ i ],
                        myUniformBuffersMemory[ i ] , true);
    }
}

void Vk_Core::myCreateDescriptorPool() {
    VkDescriptorPoolSize poolSize {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast< uint32_t >(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets       = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );  

    if (vkCreateDescriptorPool(myDevice, &poolInfo, nullptr, &myDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error( "failed to create descriptor pool!" );
    }
}

void Vk_Core::myCreateDescriptorSets() {
    std::vector< VkDescriptorSetLayout > layouts(MAX_FRAMES_IN_FLIGHT, myDescriptorSetLayout);

    // Allocation
    VkDescriptorSetAllocateInfo          allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = myDescriptorPool;
    allocInfo.descriptorSetCount = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT ) ;
    allocInfo.pSetLayouts        = layouts.data();

    myDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(myDevice, &allocInfo, myDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error( "failed to allocate descriptor sets!" );
    }

    // Configure teh descriptors
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = myUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof( UniformBufferObject );

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet          = myDescriptorSets[ i ];
        descriptorWrite.dstBinding      = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;
        vkUpdateDescriptorSets( myDevice, 1, &descriptorWrite, 0, nullptr );
    }

    
}

#pragma region Functional Functions

void Vk_Core::myCheckExtensionSupport() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > extensions( extensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );

    std::cout << "Available extensions:" << std::endl;

    for ( const VkExtensionProperties& extension : extensions ) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }
}

bool Vk_Core::myCheckValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

    std::vector< VkLayerProperties > availableLyaers( layerCount );
    vkEnumerateInstanceLayerProperties( &layerCount, availableLyaers.data() );

    std::cout << "Available validation layers:" << std::endl;

    for ( const char* layerName : myValidationLayers ) {
        bool layerFound = false;

        for ( const auto& layerProperties : availableLyaers ) {
            if ( strcmp( layerName, layerProperties.layerName ) == 0 ) {
                layerFound = true;

                std::cout << "\t" << layerName << std::endl;

                break;
            }
        }

        if ( !layerFound )
            return false;
    }

    return true;
}

void Vk_Core::SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers ) {
    myEnableValidationLayers = aEnableValidationLayers;
    myValidationLayers       = someValidationLayers;
}

void Vk_Core::SetRequiredExtension( std::vector< const char* > someDeviceExtensions ) {
    myDeviceExtensions = someDeviceExtensions;
}

bool Vk_Core::myCheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) {
    // Basic Properties
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &deviceProperties );

    // Optional Features
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures( aPhysicalDevice, &deviceFeatures );

    // Queue Families
    QueueFamilyIndices indices = myFindQueueFamilies( aPhysicalDevice );

    // Extensions
    bool extensionSupported = myCheckExtensionSupport( aPhysicalDevice );

    // Verify the swap chain support
    bool swapChainAdequate = false;
    if ( extensionSupported ) {
        SwapChainSupportDetails swapChainSupport = myQuerySwapChainSupport( aPhysicalDevice );
        swapChainAdequate                        = !swapChainSupport.myFormats.empty() && !swapChainSupport.myPresentModes.empty();
    }

    // TODO: More check options

    return indices.isComplete() && extensionSupported && swapChainAdequate;
}

QueueFamilyIndices Vk_Core::myFindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) {
    QueueFamilyIndices indices;
    uint32_t           queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        // Note: It is possible to choose a physical device that supports drawing and presentation
        // in the same queue for improved performance.

        // Check for present queue
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( aPhysicalDevice, i, mySurface, &presentSupport );
        if ( presentSupport ) {
            indices.myPresentFamily = i;
        }

        // Check for graphic queue
        if ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
            indices.myGraphicsFamily = i;
        }

        // Check for (explicit) transfer queue
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            indices.myTransferFamily = i;
        }

        if ( indices.isComplete() )
            break;

        i++;
    }

    return indices;
}

SwapChainSupportDetails Vk_Core::myQuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) {
    SwapChainSupportDetails details;

    // Step #1: Determine the supported capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( aPhysicalDevice, mySurface, &details.myCapabilities );

    // Step #2��Querying the supported surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, mySurface, &formatCount, nullptr );

    if ( formatCount != 0 ) {
        details.myFormats.resize( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, mySurface, &formatCount, details.myFormats.data() );
    }

    // Step #3: Querying the supported present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, mySurface, &presentModeCount, nullptr );

    if ( presentModeCount != 0 ) {
        details.myPresentModes.resize( presentModeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, mySurface, &presentModeCount, details.myPresentModes.data() );
    }

    return details;
}

bool Vk_Core::myCheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > availableExtensions( extensionCount );
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, availableExtensions.data() );

    std::set< std::string > requiredExtensions( myDeviceExtensions.begin(), myDeviceExtensions.end() );

    for ( const auto& extension : availableExtensions ) {
        requiredExtensions.erase( extension.extensionName );
    }

    return requiredExtensions.empty();
}

VkSurfaceFormatKHR Vk_Core::myChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) {
    for ( const auto& availableFormat : someAvailableFormats ) {
        if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
            return availableFormat;
        }
    }

    return someAvailableFormats[ 0 ];
}

VkPresentModeKHR Vk_Core::myChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) {
    // Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
    // On mobile devices, where energy usage is more important, use VK_PRESENT_MODE_FIFO_KHR
    for ( const auto& availablePresentMode : someAvailablePresentModes ) {
        if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
            return availablePresentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Vk_Core::myChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) {
    if ( aCapabilities.currentExtent.width != std::numeric_limits< uint32_t >::max() ) {
        return aCapabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize( myWindow, &width, &height );

        VkExtent2D actualExtent = { static_cast< uint32_t >( width ), static_cast< uint32_t >( height ) };

        actualExtent.width  = std::clamp( actualExtent.width, aCapabilities.minImageExtent.width, aCapabilities.maxImageExtent.width );
        actualExtent.height = std::clamp( actualExtent.height, aCapabilities.minImageExtent.height, aCapabilities.maxImageExtent.height );

        return actualExtent;
    }
}

VkShaderModule Vk_Core::myCreateShaderModule( const std::vector< char >& someShaderCode ) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = someShaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( someShaderCode.data() );

    VkShaderModule shaderModule;
    if ( vkCreateShaderModule( myDevice, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create shader module!" );
    }

    return shaderModule;
}

void Vk_Core::myRecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if ( vkBeginCommandBuffer( myGraphicsCommandBuffers[ myCurrentFrame ], &beginInfo ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to begin recording command buffer!" );
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = myRenderPass;
    renderPassInfo.framebuffer       = mySwapChainFrameBuffers[ anImageIndex ];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = mySwapChainExtent;

    VkClearValue clearColor        = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearColor;

    vkCmdBeginRenderPass( myGraphicsCommandBuffers[ myCurrentFrame ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    vkCmdBindPipeline( myGraphicsCommandBuffers[ myCurrentFrame ], VK_PIPELINE_BIND_POINT_GRAPHICS, myGfxPipeline );

    VkBuffer     vertexBuffers[] = { myVertexBuffer };
    VkDeviceSize offsets[]       = { 0 };
    vkCmdBindVertexBuffers( myGraphicsCommandBuffers[ myCurrentFrame ], 0, 1, vertexBuffers, offsets );
    vkCmdBindIndexBuffer( myGraphicsCommandBuffers[ myCurrentFrame ], myIndexBuffer, 0, VK_INDEX_TYPE_UINT16 );

    // vkCmdDraw( myGraphicsCommandBuffers[ myCurrentFrame ], static_cast< uint32_t >( vertices.size() ), 1, 0, 0 );
    vkCmdDrawIndexed( myGraphicsCommandBuffers[ myCurrentFrame ], static_cast< uint32_t >( indices.size() ), 1, 0, 0, 0 );

    vkCmdEndRenderPass( myGraphicsCommandBuffers[ myCurrentFrame ] );

    if ( vkEndCommandBuffer( myGraphicsCommandBuffers[ myCurrentFrame ] ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to record command buffer!" );
    }
}

void Vk_Core::myFramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight ) {
    auto app = reinterpret_cast< Vk_Core* >( glfwGetWindowUserPointer( aWindow ) );

    app->myFramebufferResized = true;
}

uint32_t Vk_Core::myFindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( myPhysicalDevice, &memProperties );

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( aTypeFiler & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & someProperties ) == someProperties ) {
            return i;
        }
    }

    throw std::runtime_error( "failed to find suitable memory type!" );
}

void Vk_Core::myCreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkBuffer& aBuffer, VkDeviceMemory& aBufferMemory, bool isExclusive ) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = aSize;
    bufferInfo.usage = aUsage;

    if ( isExclusive ) {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else {
        const QueueFamilyIndices indices              = myFindQueueFamilies( myPhysicalDevice );
        const uint32_t           queueFamilyIndices[] = { indices.myGraphicsFamily.value(), indices.myTransferFamily.value() };

        bufferInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
        bufferInfo.queueFamilyIndexCount = 2;
        bufferInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }

    if ( vkCreateBuffer( myDevice, &bufferInfo, nullptr, &aBuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create buffer!" );
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements( myDevice, aBuffer, &memRequirements );

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = myFindMemoryType( memRequirements.memoryTypeBits, someProperties );

    if ( vkAllocateMemory( myDevice, &allocInfo, nullptr, &aBufferMemory ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate buffer memory!" );
    }

    vkBindBufferMemory( myDevice, aBuffer, aBufferMemory, 0 );
}

void Vk_Core::myCopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = myTransferCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers( myDevice, &allocInfo, &commandBuffer );

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer( commandBuffer, &beginInfo );

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );

    vkEndCommandBuffer( commandBuffer );

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit( myTransferQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( myTransferQueue );

    vkFreeCommandBuffers( myDevice, myTransferCommandPool, 1, &commandBuffer );
}

void Vk_Core::myCopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = myGraphicsCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers( myDevice, &allocInfo, &commandBuffer );

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer( commandBuffer, &beginInfo );

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );

    vkEndCommandBuffer( commandBuffer );

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit( myTransferQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( myTransferQueue );

    vkFreeCommandBuffers( myDevice, myTransferCommandPool, 1, &commandBuffer );
}

void Vk_Core::myUpdateUniformBuffer(uint32_t aCurrentImage) {
    // Use the chrono lib to make sure the update is based on time instead of time rate
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

    UniformBufferObject ubo {};
    ubo.model = glm::rotate( glm::mat4( 1.0f ), time * glm::radians( 90.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );
    ubo.view  = glm::lookAt( glm::vec3( 2.0f, 2.0f, 2.0f ), glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );
    ubo.proj  = glm::perspective( glm::radians( 45.0f ), mySwapChainExtent.width / ( float )mySwapChainExtent.height, 0.1f, 10.0f );
    ubo.proj[ 1 ][ 1 ] *= -1;

    void* data;
    vkMapMemory( myDevice, myUniformBuffersMemory[ aCurrentImage ], 0, sizeof(ubo), 0, &data);
    memcpy( data, &ubo, sizeof(ubo) );
    vkUnmapMemory( myDevice, myUniformBuffersMemory[ aCurrentImage ] );
}
#pragma endregion

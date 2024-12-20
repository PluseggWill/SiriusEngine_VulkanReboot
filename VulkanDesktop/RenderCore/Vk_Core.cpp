#include "Vk_Core.h"
#include "../Util/Util_Loader.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_Types.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#endif

std::string vertShaderPath    = "Shader/TriangleVert.spv";
std::string fragShaderPath    = "Shader/TrianglePix.spv";
std::string fragLitShaderPath = "Shader/TriangleFrag_Lit.spv";
std::string texturePath       = "../Data/Textures/viking_room.png";
std::string houseModelPath    = "../Data/Models/viking_room.obj";
std::string monkeyModelPath   = "../Data/Models/monkey_smooth.obj";

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
    Clear();
}

void Vk_Core::Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Clear();
}

void Vk_Core::InitWindow() {
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

    myWindow = glfwCreateWindow( myWidth, myHeight, "Vulkan Window", nullptr, nullptr );
    glfwSetWindowUserPointer( myWindow, this );
    glfwSetFramebufferSizeCallback( myWindow, FramebufferResizeCallback );
    glfwSetKeyCallback( myWindow, HandleInputCallback );
}

void Vk_Core::MainLoop() {
    while ( !glfwWindowShouldClose( myWindow ) ) {
        // Process the events that have already been received and then returns immediately
        glfwPollEvents();

        DrawFrame( myFrameDatas[ myCurrentFrame ] );
        myFrameNumber++;
        myCurrentFrame = myFrameNumber % MAX_FRAMES_IN_FLIGHT;
    }

    // Make sure the GPU has stopped doing things.
    vkDeviceWaitIdle( myDevice );
}

void Vk_Core::Clear() {
    // All other Vulkan resources should be cleaned up
    // before the instance is destroyed.

    mySwapChainDeletionQueue.flush();

    myDeletionQueue.flush();

    vkDestroyCommandPool( myDevice, myGraphicsCommandPool, nullptr );
    vkDestroyCommandPool( myDevice, myTransferCommandPool, nullptr );

    vkDestroyDevice( myDevice, nullptr );

    vkDestroySurfaceKHR( myInstance, mySurface, nullptr );

    vkDestroyInstance( myInstance, nullptr );

    glfwDestroyWindow( myWindow );

    glfwTerminate();
}

void Vk_Core::InitVulkan() {
    // Part 1: Base
    CreateInstance();
    // TODO: Set up debug messenger
    CreateSurface();
    PickPhysicalDevice();
    InitQueueFamilyIndices();
    CreateLogicalDevice();
    CreateCommandPool();
    InitAllocator();

    // Part 2: Swap Chain
    CreateSwapChain();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGfxPipeline();

    // TODO: Remove this
    CreateMaterial( myBasicPipeline, myPipelineLayout, 0 );

    CreateColorResources();
    CreateDepthResources();
    CreateFrameBuffers();

    // Part 3: Prepare for draw frames resources
    CreateTexture( texturePath, 0 );
    CreateTextureSampler();
    {
        CreateMesh( houseModelPath, 0 );
        CreateMesh( monkeyModelPath, 1 );
    }
    CreateFrameData();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCamera();
}

void Vk_Core::CreateInstance() {
    // Check validation layer first
    if ( myEnableValidationLayers && !CheckValidationLayerSupport() ) {
        throw std::runtime_error( "validation layers requested, but not available!" );
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Hello Vulkan";
    appInfo.applicationVersion = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.pEngineName        = "Sirius Engine";
    appInfo.engineVersion      = VK_MAKE_API_VERSION( 0, 1, 0, 0 );
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    uint32_t     glfwExtensionCount = 0;
    const char** glfwExtensions     = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
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
    CheckExtensionSupport();
#endif  // _DEBUG

    // VkResult result = vkCreateInstance(&createInfo, nullptr, &myInstance);
    if ( vkCreateInstance( &createInfo, nullptr, &myInstance ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create instance!" );
    }
}

void Vk_Core::PickPhysicalDevice() {
    // The selected GPU will be stored in a VkPhysicalDevice handle
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, nullptr );

    if ( deviceCount == 0 )
        throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

    std::vector< VkPhysicalDevice > devices( deviceCount );
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices ) {
        if ( CheckDeviceSuitable( device ) ) {
            myPhysicalDevice = device;
            myMSAASamples    = GetMaxUsableSampleCount();
            break;
        }
    }

    if ( myPhysicalDevice == VK_NULL_HANDLE )
        throw std::runtime_error( "failed to find a suitable GPU!" );
}

void Vk_Core::CreateLogicalDevice() {
    // Step #1: Specifying the queues to be created

    std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;
    std::set< uint32_t >                   uniqueQueueFamilies = { myQueueFamilyIndices.myGraphicsFamily.value(), myQueueFamilyIndices.myPresentFamily.value(),
                                                 myQueueFamilyIndices.myTransferFamily.value() };

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
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fillModeNonSolid  = VK_TRUE;

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
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myGraphicsFamily.value(), 0, &myGraphicsQueue );
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myPresentFamily.value(), 0, &myPresentQueue );
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myTransferFamily.value(), 0, &myTransferQueue );
}

void Vk_Core::CreateSurface() {
    if ( glfwCreateWindowSurface( myInstance, myWindow, nullptr, &mySurface ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create window surface!" );
    }
}

void Vk_Core::InitAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = myPhysicalDevice;
    allocatorInfo.device         = myDevice;
    allocatorInfo.instance       = myInstance;
    vmaCreateAllocator( &allocatorInfo, &myAllocator );

    // Deletion Queue
    myDeletionQueue.pushFunction( [ = ]() { vmaDestroyAllocator( myAllocator ); } );
}

void Vk_Core::CreateSwapChain() {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( myPhysicalDevice );

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat( swapChainSupport.myFormats );
    VkPresentModeKHR   presentMode   = ChooseSwapPresentMode( swapChainSupport.myPresentModes );
    VkExtent2D         extent        = ChooseSwapExtent( swapChainSupport.myCapabilities );

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

    const uint32_t queueFamilyIndices[] = { myQueueFamilyIndices.myGraphicsFamily.value(), myQueueFamilyIndices.myPresentFamily.value() };

    if ( myQueueFamilyIndices.myGraphicsFamily != myQueueFamilyIndices.myPresentFamily ) {
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

    // Create swap chain image views
    mySwapChainImageViews.resize( imageCount );

    for ( size_t i = 0; i < imageCount; i++ ) {
        mySwapChainImageViews[ i ] = CreateImageView( mySwapChainImages[ i ], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT );
    }

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() {
        for ( VkImageView imageView : mySwapChainImageViews )
            vkDestroyImageView( myDevice, imageView, nullptr );

        vkDestroySwapchainKHR( myDevice, mySwapChain, nullptr );
    } );
}

void Vk_Core::CreateGfxPipeline() {
    // Step #1 & 2: Load & Create shader module
    VkShaderModule vertShaderModule = CreateShaderModule( vertShaderPath );

    // VkShaderModule fragShaderModule = CreateShaderModule( fragLitShaderPath );
    VkShaderModule fragShaderModule = CreateShaderModule( fragShaderPath );

    // Step #3: Create shader stage info
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = VkInit::Pipeline_VertexShaderStageCreateInfo( vertShaderModule );

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = VkInit::Pipeline_PixelShaderStageCreateInfo( fragShaderModule );

    // Step #4: Create vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkInit::Pipeline_VertexInputStateCreateInfo();

    auto bindingDescription   = Vertex::getBindingDescription();
    auto attributeDescription = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescription.data();

    // Step #5: Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );

    // Step #6: Viewports and Scissors
    // Viewports define the transformation from the image to the framebuffer
    VkViewport viewport = VkInit::ViewportCreateInfo( mySwapChainExtent );

    // Scissors define the in which regions pixels will actually be stored
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = mySwapChainExtent;

    // Step #7: Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = VkInit::Pipeline_RasterizationCreateInfo( FILL_MODE_LINE ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL );

    // Step #8: Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = VkInit::Pipeline_MultisampleCreateInfo( myMSAASamples );

    // Step #9: Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = VkInit::Pipeline_DepthStencilCreateInfo();

    // Step #10: Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );

    // Step #11: Dynamic state (not using at this moment)
    std::vector< VkDynamicState > dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

    VkPipelineDynamicStateCreateInfo dynamicState = VkInit::Pipeline_DynamicStateCreateInfo();

    // Step #12: Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutCreateInfo.setLayoutCount             = 1;
    pipelineLayoutCreateInfo.pSetLayouts                = &myGlobalSetLayout;

    if ( vkCreatePipelineLayout( myDevice, &pipelineLayoutCreateInfo, nullptr, &myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create pipeline layout!" );
    }

    // Step #13: Combine
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( vertShaderStageInfo );
    pipelineBuilder.myShaderStages.push_back( fragShaderStageInfo );
    pipelineBuilder.myVertexInputInfo      = vertexInputInfo;
    pipelineBuilder.myInputAssembly        = inputAssembly;
    pipelineBuilder.myViewport             = viewport;
    pipelineBuilder.myScissor              = scissor;
    pipelineBuilder.myRasterizer           = rasterizer;
    pipelineBuilder.myMultisampling        = multisampling;
    pipelineBuilder.myDepthStencil         = depthStencilInfo;
    pipelineBuilder.myColorBlendAttachment = colorBlendAttachment;
    pipelineBuilder.myDynamicState         = dynamicState;
    pipelineBuilder.myPipelineLayout       = myPipelineLayout;

    myBasicPipeline = pipelineBuilder.BuildPipeline( myDevice, myRenderPass );

    vkDestroyShaderModule( myDevice, vertShaderModule, nullptr );
    vkDestroyShaderModule( myDevice, fragShaderModule, nullptr );

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() {
        vkDestroyPipeline( myDevice, myBasicPipeline, nullptr );
        vkDestroyPipelineLayout( myDevice, myPipelineLayout, nullptr );
    } );
}

void Vk_Core::CreateRenderPass() {
    // Step #1: Attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = mySwapChainImageFormat;
    colorAttachment.samples        = myMSAASamples;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Step #2: Create depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = FindDepthFormat();
    depthAttachment.samples        = myMSAASamples;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Step #3: Create resolve attachment & reference
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format         = mySwapChainImageFormat;
    colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Step #4: Subpasses and attachment references
    VkAttachmentReference colorAttachmentRefs{};
    colorAttachmentRefs.attachment = 0;
    colorAttachmentRefs.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRefs;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments     = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Step #5: Render pass
    std::array< VkAttachmentDescription, 3 > attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };
    VkRenderPassCreateInfo                   renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if ( vkCreateRenderPass( myDevice, &renderPassInfo, nullptr, &myRenderPass ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create render pass!" );
    }

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() { vkDestroyRenderPass( myDevice, myRenderPass, nullptr ); } );
}

void Vk_Core::CreateFrameBuffers() {
    mySwapChainFrameBuffers.resize( mySwapChainImageViews.size() );

    for ( size_t i = 0; i < mySwapChainImageViews.size(); i++ ) {
        std::array< VkImageView, 3 > attachments = {
            myColorTexture.ImageView(),
            myDepthTexture.ImageView(),
            mySwapChainImageViews[ i ],
        };

        VkFramebufferCreateInfo frameBufferInfo{};
        frameBufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.renderPass      = myRenderPass;
        frameBufferInfo.attachmentCount = static_cast< uint32_t >( attachments.size() );
        frameBufferInfo.pAttachments    = attachments.data();
        frameBufferInfo.width           = mySwapChainExtent.width;
        frameBufferInfo.height          = mySwapChainExtent.height;
        frameBufferInfo.layers          = 1;

        if ( vkCreateFramebuffer( myDevice, &frameBufferInfo, nullptr, &mySwapChainFrameBuffers[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create framebuffer!" );
        }
    }

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() {
        for ( VkFramebuffer frameBuffer : mySwapChainFrameBuffers )
            vkDestroyFramebuffer( myDevice, frameBuffer, nullptr );
    } );
}

void Vk_Core::CreateFrameData() {
    myFrameDatas.resize( MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        // Step #1: Create Command Buffers
        const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( myGraphicsCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );

        if ( vkAllocateCommandBuffers( myDevice, &allocInfo, &myFrameDatas[ i ].myCommandBuffer ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate command buffers!" );
        }

        // Step #2: Create Camera Buffers
        VkDeviceSize bufferSize = sizeof( GpuCameraData );

        CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameDatas[ i ].myCameraBuffer, true );

        // Step #3: Create SyncObjects
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if ( vkCreateSemaphore( myDevice, &semaphoreInfo, nullptr, &myFrameDatas[ i ].myPresentSemaphore ) != VK_SUCCESS
             || vkCreateSemaphore( myDevice, &semaphoreInfo, nullptr, &myFrameDatas[ i ].myRenderSemaphore ) != VK_SUCCESS
             || vkCreateFence( myDevice, &fenceInfo, nullptr, &myFrameDatas[ i ].myRenderFence ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create semaphores/fence!" );
        }

        // Deletion Queue
        myDeletionQueue.pushFunction( [ = ]() {
            vmaDestroyBuffer( myAllocator, myFrameDatas[ i ].myCameraBuffer.myBuffer, myFrameDatas[ i ].myCameraBuffer.myAllocation );
            vkDestroySemaphore( myDevice, myFrameDatas[ i ].myPresentSemaphore, nullptr );
            vkDestroySemaphore( myDevice, myFrameDatas[ i ].myRenderSemaphore, nullptr );
            vkDestroyFence( myDevice, myFrameDatas[ i ].myRenderFence, nullptr );
        } );
    }
}

void Vk_Core::CreateUniformBuffers() {
    // TODO: Temp env data creation
    const size_t envDataBufferSize = MAX_FRAMES_IN_FLIGHT * PadUniformBufferSize( sizeof( GpuEnvironmentData ) );
    CreateBuffer( envDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, myEnvDataBuffer, true );

    myDeletionQueue.pushFunction( [ = ]() { vmaDestroyBuffer( myAllocator, myEnvDataBuffer.myBuffer, myEnvDataBuffer.myAllocation ); } );
}

void Vk_Core::CreateCommandPool() {

    // Graphic queue command pool
    VkCommandPoolCreateInfo poolInfo = VkInit::CommandPoolCreateInfo( myQueueFamilyIndices.myGraphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myGraphicsCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphic command pool!" );
    }

    // Tranfer queue command pool
    poolInfo = VkInit::CommandPoolCreateInfo( myQueueFamilyIndices.myTransferFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myTransferCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create transfer command pool!" );
    }
}

void Vk_Core::DrawFrame( const FrameData aFrameData ) {
    vkWaitForFences( myDevice, 1, &aFrameData.myRenderFence, VK_TRUE, UINT64_MAX );

    uint32_t imageIndex;
    // Once the image is available for the next frame, myPresentSemaphore will be signaled and ready to be acquired
    VkResult result = vkAcquireNextImageKHR( myDevice, mySwapChain, UINT64_MAX, aFrameData.myPresentSemaphore, VK_NULL_HANDLE, &imageIndex );

    if ( result == VK_ERROR_OUT_OF_DATE_KHR ) {
        // Swap chain incompatible, recreate the swap chain
        RecreateSwapChain();
        return;
    }
    else if ( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR ) {
        throw std::runtime_error( "failed to acquire swap chain image!" );
    }

    // TODO: make the parameters only access the frame data
    UpdateUniformBuffer( myCurrentFrame );

    // Only reset the fence if we are submitting work
    vkResetFences( myDevice, 1, &aFrameData.myRenderFence );

    vkResetCommandBuffer( aFrameData.myCommandBuffer, 0 );
    RecordCommandBuffer( aFrameData.myCommandBuffer, imageIndex );

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore          waitSemaphore[] = { aFrameData.myPresentSemaphore };
    VkPipelineStageFlags waitStages[]    = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount        = 1;
    submitInfo.pWaitSemaphores           = waitSemaphore;
    submitInfo.pWaitDstStageMask         = waitStages;
    submitInfo.commandBufferCount        = 1;
    submitInfo.pCommandBuffers           = &aFrameData.myCommandBuffer;

    VkSemaphore signalSemaphores[]  = { aFrameData.myRenderSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if ( vkQueueSubmit( myGraphicsQueue, 1, &submitInfo, aFrameData.myRenderFence ) != VK_SUCCESS ) {
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
        RecreateSwapChain();
    }
    else if ( result != VK_SUCCESS ) {
        throw std::runtime_error( "failed to present swap chain image!" );
    }
}

void Vk_Core::CreateCamera() {
    myCamera.SetLens( 45.0f, 0.1f, 10.0f, static_cast< float >( mySwapChainExtent.width ) / static_cast< float >( mySwapChainExtent.height ) );
    myCamera.LookAt( glm::vec3( 0.0f, 3.0f, 2.0f ), glm::vec3( 0.0f, 0.0f, 0.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );
}

void Vk_Core::RecreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize( myWindow, &width, &height );
    while ( width == 0 || height == 0 ) {
        glfwGetFramebufferSize( myWindow, &width, &height );
        // Put the thread to sleep until at least one event has been received and then processe all received events.
        glfwWaitEvents();
    }

    vkDeviceWaitIdle( myDevice );

    mySwapChainDeletionQueue.flush();

    CreateSwapChain();
    CreateRenderPass();
    CreateGfxPipeline();
    CreateColorResources();
    CreateDepthResources();
    CreateFrameBuffers();

    // Reset the camera's aspect
    myCamera.SetAspect( static_cast< float >( mySwapChainExtent.width ) / static_cast< float >( mySwapChainExtent.height ) );
}

void Vk_Core::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding =
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, eVk_CameraBinding );

    VkDescriptorSetLayoutBinding gpuEnvDataLayoutBinding =
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, eVk_EnvBinding );

    /*VkDescriptorSetLayoutBinding samplerLayoutBinding =
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 );*/

    std::array< VkDescriptorSetLayoutBinding, 2 > bindings = { uboLayoutBinding, gpuEnvDataLayoutBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext        = nullptr;
    layoutInfo.flags        = 0;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();

    if ( vkCreateDescriptorSetLayout( myDevice, &layoutInfo, nullptr, &myGlobalSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create descriptor set layout!" );
    }

    myDeletionQueue.pushFunction( [ = ]() { vkDestroyDescriptorSetLayout( myDevice, myGlobalSetLayout, nullptr ); } );
}

void Vk_Core::CreateDescriptorPool() {
    std::array< VkDescriptorPoolSize, 2 > poolSizes{};
    poolSizes[ 0 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[ 0 ].descriptorCount = 10;
    poolSizes[ 1 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[ 1 ].descriptorCount = 10;
    // poolSizes[ 1 ].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // poolSizes[ 1 ].descriptorCount = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );

    if ( vkCreateDescriptorPool( myDevice, &poolInfo, nullptr, &myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create descriptor pool!" );
    }

    // Deletion Queue
    myDeletionQueue.pushFunction( [ = ]() { vkDestroyDescriptorPool( myDevice, myDescriptorPool, nullptr ); } );
}

void Vk_Core::CreateDescriptorSets() {
    // Configure the descriptors
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        // Allocation, one descriptor set per frame data
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = myDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &myGlobalSetLayout;

        if ( vkAllocateDescriptorSets( myDevice, &allocInfo, &myFrameDatas[ i ].myGlobalDescriptor ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate descriptor sets!" );
        }

        // Descriptor Writes
        std::array< VkWriteDescriptorSet, 2 > descriptorWrites{};

        VkDescriptorBufferInfo camBufferInfo{};
        camBufferInfo.buffer = myFrameDatas[ i ].myCameraBuffer.myBuffer;
        camBufferInfo.offset = 0;
        camBufferInfo.range  = sizeof( GpuCameraData );

        VkDescriptorBufferInfo envBufferInfo{};
        envBufferInfo.buffer = myEnvDataBuffer.myBuffer;
        envBufferInfo.offset = 0;
        envBufferInfo.range  = sizeof( GpuEnvironmentData );

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = myTextureMap[ 0 ].ImageView();
        imageInfo.sampler     = myTextureSampler;

        descriptorWrites[ 0 ] =
            VkInit::DescriptorSetWriteCreateInfo( myFrameDatas[ i ].myGlobalDescriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &camBufferInfo, eVk_CameraBinding, 1 );
        descriptorWrites[ 1 ] =
            VkInit::DescriptorSetWriteCreateInfo( myFrameDatas[ i ].myGlobalDescriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &envBufferInfo, eVk_EnvBinding, 1 );
        // descriptorWrites[ 1 ] = VkInit::DescriptorSetWriteCreateInfo( myFrameDatas[ i ].myGlobalDescriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, 1, 1 );

        vkUpdateDescriptorSets( myDevice, static_cast< uint32_t >( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
    }
}

void Vk_Core::CreateTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties( myPhysicalDevice, &properties );
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast< float >( myTextureImageMipLevels );

    if ( vkCreateSampler( myDevice, &samplerInfo, nullptr, &myTextureSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create texture sampler!" );
    }

    // Deletion Queue
    myDeletionQueue.pushFunction( [ = ]() { vkDestroySampler( myDevice, myTextureSampler, nullptr ); } );
}

void Vk_Core::CreateDepthResources() {
    const VkFormat depthFormat = FindDepthFormat();

    CreateImage( mySwapChainExtent, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1, myMSAASamples,
                 myDepthTexture.AllocImage() );
    myDepthTexture.ImageView() = CreateImageView( myDepthTexture.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );

    TransitionImageLayout( myDepthTexture.Image(), depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1 );

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() {
        vkDestroyImageView( myDevice, myDepthTexture.ImageView(), nullptr );
        vmaDestroyImage( myAllocator, myDepthTexture.Image(), myDepthTexture.Allocation() );
    } );
}

void Vk_Core::CreateColorResources() {
    const VkFormat colorFormat = mySwapChainImageFormat;

    CreateImage( mySwapChainExtent, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                 VMA_MEMORY_USAGE_GPU_ONLY, 1, myMSAASamples, myColorTexture.AllocImage() );
    myColorTexture.ImageView() = CreateImageView( myColorTexture.Image(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT );

    // Deletion Queue
    mySwapChainDeletionQueue.pushFunction( [ = ]() {
        vkDestroyImageView( myDevice, myColorTexture.ImageView(), nullptr );
        vmaDestroyImage( myAllocator, myColorTexture.Image(), myColorTexture.Allocation() );
    } );
}

void Vk_Core::InitQueueFamilyIndices() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( myPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( myPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        // Note: It is possible to choose a physical device that supports drawing and presentation
        // in the same queue for improved performance.

        // Check for present queue & graphic queue
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( myPhysicalDevice, i, mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myQueueFamilyIndices.myPresentFamily  = i;
            myQueueFamilyIndices.myGraphicsFamily = i;
        }

        // Check for (explicit) transfer queue
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myQueueFamilyIndices.myTransferFamily = i;
        }

        if ( myQueueFamilyIndices.isComplete() )
            break;

        i++;
    }
}

void Vk_Core::DrawObjects( VkCommandBuffer aCommandBuffer, std::vector< RenderObject >& someRenderObjects, uint32_t anImageIndex ) {
    Mesh*     lastMesh     = nullptr;
    Material* lastMaterial = nullptr;

    for ( auto renderObject = someRenderObjects.begin(); renderObject != someRenderObjects.end(); renderObject++ ) {
        // Bind pipeline based on the mesh material
        if ( renderObject->myMaterial != lastMaterial ) {
            vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderObject->myMaterial->myPipeline );

            lastMaterial = renderObject->myMaterial;
        }

        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

        GpuCameraData ubo{};
        ubo.model = glm::rotate( glm::mat4( 1.0f ), ENABLE_ROTATE ? time * glm::radians( 90.0f ) : 0, glm::vec3( 0.0f, 0.0f, 1.0f ) );
        ubo.view  = myCamera.myView;
        ubo.proj  = myCamera.myProj;

        // TODO:
    }
}

#pragma region Functional Functions

void Vk_Core::CheckExtensionSupport() const {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > extensions( extensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );

    std::cout << "Available extensions:" << std::endl;

    for ( const VkExtensionProperties& extension : extensions ) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }
}

bool Vk_Core::CheckValidationLayerSupport() const {
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

bool Vk_Core::CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const {
    // Basic Properties
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &myPhysicalDeviceProperties );

    // Optional Features
    vkGetPhysicalDeviceFeatures( aPhysicalDevice, &myPhysicalDeviceFeatures );

    // Queue Families
    QueueFamilyIndices indices = FindQueueFamilies( aPhysicalDevice );

    // Extensions
    bool extensionSupported = CheckExtensionSupport( aPhysicalDevice );

    // Verify the swap chain support
    bool swapChainAdequate = false;
    if ( extensionSupported ) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( aPhysicalDevice );
        swapChainAdequate                        = !swapChainSupport.myFormats.empty() && !swapChainSupport.myPresentModes.empty();
    }

#ifdef _DEBUG
    std::cout << "The GPU has a minimum buffer alignment of " << myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment << std::endl;
#endif  // _DEBUG

    // TODO: More check options

    return indices.isComplete() && extensionSupported && swapChainAdequate && myPhysicalDeviceFeatures.samplerAnisotropy;
}

QueueFamilyIndices Vk_Core::FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const {
    QueueFamilyIndices indices;
    uint32_t           queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        // Note: It is possible to choose a physical device that supports drawing and presentation
        // in the same queue for improved performance.

        // Check for present queue & graphic queue
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( aPhysicalDevice, i, mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            indices.myPresentFamily  = i;
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

SwapChainSupportDetails Vk_Core::QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const {
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

bool Vk_Core::CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const {
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

VkSurfaceFormatKHR Vk_Core::ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) const {
    for ( const auto& availableFormat : someAvailableFormats ) {
        if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
            return availableFormat;
        }
    }

    return someAvailableFormats[ 0 ];
}

VkPresentModeKHR Vk_Core::ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) const {
    // Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
    // On mobile devices, where energy usage is more important, use VK_PRESENT_MODE_FIFO_KHR
    for ( const auto& availablePresentMode : someAvailablePresentModes ) {
        if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
            return availablePresentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Vk_Core::ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) const {
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

VkShaderModule Vk_Core::CreateShaderModule( const std::vector< char >& someShaderCode ) const {
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

VkShaderModule Vk_Core::CreateShaderModule( const std::string aShaderPath ) const {
    const auto shaderCode = UtilLoader::ReadFile( aShaderPath );

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( shaderCode.data() );

    VkShaderModule shaderModule;
    if ( vkCreateShaderModule( myDevice, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create shader module!" );
    }

    return shaderModule;
}

void Vk_Core::RecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( 0 );

    if ( vkBeginCommandBuffer( aCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to begin recording command buffer!" );
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = myRenderPass;
    renderPassInfo.framebuffer       = mySwapChainFrameBuffers[ anImageIndex ];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = mySwapChainExtent;

    std::array< VkClearValue, 2 > clearValues{};
    clearValues[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[ 1 ].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast< uint32_t >( clearValues.size() );
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, myBasicPipeline );

    VkBuffer     vertexBuffers[] = { myMeshMap[ 0 ].myVertexBuffer.myBuffer };
    VkDeviceSize offsets[]       = { 0 };

    vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
    vkCmdBindIndexBuffer( aCommandBuffer, myMeshMap[ 0 ].myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

    // Offset for our environment buffer
    uint32_t uniform_offset = PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * myCurrentFrame;

    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, myPipelineLayout, 0, 1, &myFrameDatas[ myCurrentFrame ].myGlobalDescriptor, 1, &uniform_offset );
    vkCmdDrawIndexed( aCommandBuffer, static_cast< uint32_t >( myMeshMap[ 0 ].myIndices.size() ), 1, 0, 0, 0 );

    vkCmdEndRenderPass( aCommandBuffer );

    if ( vkEndCommandBuffer( aCommandBuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to record command buffer!" );
    }
}

void Vk_Core::FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight ) {
    const auto vkCore = reinterpret_cast< Vk_Core* >( glfwGetWindowUserPointer( aWindow ) );

    vkCore->myFramebufferResized = true;
}

uint32_t Vk_Core::FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( myPhysicalDevice, &memProperties );

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( aTypeFiler & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & someProperties ) == someProperties ) {
            return i;
        }
    }

    throw std::runtime_error( "failed to find suitable memory type!" );
}

void Vk_Core::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, AllocatedBuffer& aBuffer, bool isExclusive ) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = aSize;
    bufferInfo.usage = aBufferUsage;

    if ( isExclusive ) {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else {
        const uint32_t queueFamilyIndices[] = { myQueueFamilyIndices.myGraphicsFamily.value(), myQueueFamilyIndices.myTransferFamily.value() };

        bufferInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
        bufferInfo.queueFamilyIndexCount = 2;
        bufferInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = aMemoryUsage;

    if ( vmaCreateBuffer( myAllocator, &bufferInfo, &vmaAllocInfo, &aBuffer.myBuffer, &aBuffer.myAllocation, nullptr ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create buffer through VMA!" );
    }
}

void Vk_Core::CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myTransferCommandPool );

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );

    EndSingleTimeCommands( commandBuffer, myTransferCommandPool, myTransferQueue );
}

void Vk_Core::CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool );

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = aSize;
    vkCmdCopyBuffer( commandBuffer, aSrcBuffer, aDstBuffer, 1, &copyRegion );

    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue );
}

void Vk_Core::UpdateUniformBuffer( uint32_t aCurrentFrame ) const {
    // Use the chrono lib to make sure the update is based on time instead of time rate
    static auto startTime = std::chrono::high_resolution_clock::now();

    const auto  currentTime = std::chrono::high_resolution_clock::now();
    const float time        = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

    // Upadate camera buffer
    GpuCameraData cam{};
    cam.model = glm::rotate( glm::mat4( 1.0f ), ENABLE_ROTATE ? time * glm::radians( 90.0f ) : 0, glm::vec3( 0.0f, 0.0f, 1.0f ) );
    cam.view  = myCamera.myView;
    cam.proj  = myCamera.myProj;

    void* data;
    vmaMapMemory( myAllocator, myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation, &data );
    memcpy( data, &cam, sizeof( cam ) );
    vmaUnmapMemory( myAllocator, myFrameDatas[ aCurrentFrame ].myCameraBuffer.myAllocation );

    // Update environment buffer
    // TODO: Figure out how dynamic buffer works :)
    float framed = ( myFrameNumber / 120.f );

    GpuEnvironmentData env{};
    env.myAmbientColor      = { sin( framed ), 0, cos( framed ), 1 };
    env.myFogColor          = { 1, 1, 1, 1 };
    env.myFogDistance       = { 1, 1, 1, 1 };
    env.mySunlightDirection = { 1, 1, 1, 1 };
    env.mySunlightColor     = { 1, 1, 1, 1 };

    char* mapGpuEnvData;
    vmaMapMemory( myAllocator, myEnvDataBuffer.myAllocation, ( void** )&mapGpuEnvData );

    mapGpuEnvData += PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * aCurrentFrame;

    memcpy( mapGpuEnvData, &env, sizeof( GpuEnvironmentData ) );
    vmaUnmapMemory( myAllocator, myEnvDataBuffer.myAllocation );
}

void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                           AllocatedImage& anImage ) const {
    CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, 1, VK_SAMPLE_COUNT_1_BIT, anImage );
}

void Vk_Core::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, AllocatedImage& anImage ) const {
    VkExtent3D extent = { anExtent.width, anExtent.height, 1 };

    CreateImage( extent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, AllocatedImage& anImage ) const {
    VkImageCreateInfo imageInfo = VkInit::ImageCreateInfo( aFormat, anImageUsage, anExtent );

    imageInfo.mipLevels   = aMipLevel;
    imageInfo.tiling      = aTiling;
    imageInfo.samples     = aNumSamples;
    imageInfo.flags       = 0;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // TODO: Consider the situation when the image can be accessed by two different queues.

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = aMemoryUsage;

    if ( vmaCreateImage( myAllocator, &imageInfo, &vmaAllocInfo, &anImage.myImage, &anImage.myAllocation, nullptr ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create image through VMA!" );
    }
}

VkCommandBuffer Vk_Core::BeginSingleTimeCommands( VkCommandPool aCommandPool ) const {
    const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( aCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers( myDevice, &allocInfo, &commandBuffer );

    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

    vkBeginCommandBuffer( commandBuffer, &beginInfo );

    return commandBuffer;
}

void Vk_Core::EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const {
    vkEndCommandBuffer( aCommandBuffer );

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &aCommandBuffer;

    vkQueueSubmit( aQueue, 1, &submitInfo, VK_NULL_HANDLE );
    vkQueueWaitIdle( aQueue );

    vkFreeCommandBuffers( myDevice, aCommandPool, 1, &aCommandBuffer );
}

void Vk_Core::TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool );

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = anOldLayout;
    barrier.newLayout                       = aNewLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = aImage;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = aMipLevel;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    if ( aNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if ( HasStencilComponent( aFormat ) ) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && aNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if ( anOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && aNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        throw std::invalid_argument( "unsupported layout transition!" );
    }

    vkCmdPipelineBarrier( commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier );

    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue );
}

void Vk_Core::CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const {
    VkCommandBuffer   commandBuffer = BeginSingleTimeCommands( myTransferCommandPool );
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { aWidth, aHeight, 1 };

    vkCmdCopyBufferToImage( commandBuffer, aBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

    EndSingleTimeCommands( commandBuffer, myTransferCommandPool, myTransferQueue );
}

VkImageView Vk_Core::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    VkImageViewCreateInfo viewInfo = VkInit::ImageViewCreateInfo( aFormat, anImage, anAspect, aMipLevel );

    VkImageView imageView;
    if ( vkCreateImageView( myDevice, &viewInfo, nullptr, &imageView ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create image view!" );
    }

    return imageView;
}

VkFormat Vk_Core::FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const {
    for ( const VkFormat format : someCandidates ) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties( myPhysicalDevice, format, &properties );
        if ( aTiling == VK_IMAGE_TILING_LINEAR && ( properties.linearTilingFeatures & someFeatures ) == someFeatures ) {
            return format;
        }
        else if ( aTiling == VK_IMAGE_TILING_OPTIMAL && ( properties.optimalTilingFeatures & someFeatures ) == someFeatures ) {
            return format;
        }
    }

    throw std::runtime_error( "failed to find supported format!" );
}

VkFormat Vk_Core::FindDepthFormat() const {
    return FindSupportedFormat( { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL,
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
}

bool Vk_Core::HasStencilComponent( VkFormat aFormat ) const {
    return aFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || aFormat == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Vk_Core::GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const {

    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties( myPhysicalDevice, aImageFormat, &formatProperties );
    if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) ) {
        throw std::runtime_error( "texture image does not support linear blitting!" );
    }

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands( myGraphicsCommandPool );

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = aImage;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = aTexWidth;
    int32_t mipHeight = aTexHeight;

    for ( uint32_t i = 1; i < aMipLevel; i++ ) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

        VkImageBlit blit{};
        blit.srcOffsets[ 0 ]               = { 0, 0, 0 };
        blit.srcOffsets[ 1 ]               = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[ 0 ]               = { 0, 0, 0 };
        blit.dstOffsets[ 1 ]               = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage( commandBuffer, aImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR );

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

        mipWidth  = mipWidth > 1 ? mipWidth / 2 : mipWidth;
        mipHeight = mipHeight > 1 ? mipHeight / 2 : mipHeight;
    }
    barrier.subresourceRange.baseMipLevel = aMipLevel - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier );

    EndSingleTimeCommands( commandBuffer, myGraphicsCommandPool, myGraphicsQueue );
}

VkSampleCountFlagBits Vk_Core::GetMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties( myPhysicalDevice, &physicalDeviceProperties );

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if ( counts & VK_SAMPLE_COUNT_64_BIT )
        return VK_SAMPLE_COUNT_64_BIT;
    if ( counts & VK_SAMPLE_COUNT_32_BIT )
        return VK_SAMPLE_COUNT_32_BIT;
    if ( counts & VK_SAMPLE_COUNT_16_BIT )
        return VK_SAMPLE_COUNT_16_BIT;
    if ( counts & VK_SAMPLE_COUNT_8_BIT )
        return VK_SAMPLE_COUNT_8_BIT;
    if ( counts & VK_SAMPLE_COUNT_4_BIT )
        return VK_SAMPLE_COUNT_4_BIT;
    if ( counts & VK_SAMPLE_COUNT_2_BIT )
        return VK_SAMPLE_COUNT_2_BIT;

    return VK_SAMPLE_COUNT_1_BIT;
}

void Vk_Core::HandleInputCallback( GLFWwindow* aWindow, int aKey, int aScanCode, int anAction, int aMode ) {
    if ( anAction == GLFW_PRESS || anAction == GLFW_REPEAT ) {
        switch ( aKey ) {
        case GLFW_KEY_W:
            // std::cout << "Moving Forward!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( 0.0f, 1.0f, 0.0f ) * SPEED );
            break;
        case GLFW_KEY_S:
            // std::cout << "Moving Backward!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( 0.0f, -1.0f, 0.0f ) * SPEED );
            break;
        case GLFW_KEY_A:
            // std::cout << "Moving Left!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( -1.0f, 0.0f, 0.0f ) * SPEED );
            break;
        case GLFW_KEY_D:
            // std::cout << "Moving Right!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( 1.0f, 0.0f, 0.0f ) * SPEED );
            break;
        case GLFW_KEY_E:
            // std::cout << "Moving Up!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( 0.0f, 0.0f, -1.0f ) * SPEED );
            break;
        case GLFW_KEY_Q:
            // std::cout << "Moving Down!" << std::endl;
            Vk_Core::GetInstance().myCamera.Move( glm::vec3( 0.0f, 0.0f, 1.0f ) * SPEED );
            break;
        default:
            break;
        }
    }
}

size_t Vk_Core::PadUniformBufferSize( size_t anOriginalSize ) const {
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize     = anOriginalSize;

    if ( minUboAlignment > 0 )
        alignedSize = ( alignedSize + minUboAlignment - 1 ) & ~( minUboAlignment - 1 );

    return alignedSize;
}

#pragma endregion

#pragma region View Data Functions:

Material* Vk_Core::CreateMaterial( VkPipeline aPipeline, VkPipelineLayout aLayout, const uint32_t anIndex ) {
    Material tempMat;

    tempMat.myPipeline       = aPipeline;
    tempMat.myPipelineLayout = aLayout;
    myMaterialMap[ anIndex ] = tempMat;

    return &myMaterialMap[ anIndex ];
}

Material* Vk_Core::GetMaterial( const uint32_t anIndex ) {
    auto it = myMaterialMap.find( anIndex );
    if ( it == myMaterialMap.end() ) {
        return nullptr;
    }
    else {
        return &( *it ).second;
    }
}

Mesh* Vk_Core::CreateMesh( const std::string& aFilename, const uint32_t anIndex ) {
    Mesh tempMesh;

    tempMesh.LoadMesh( aFilename );

    tempMesh.BuildBuffers();

    myMeshMap[ anIndex ] = tempMesh;

    // Deletion Queue
    myDeletionQueue.pushFunction( [ = ]() {
        vmaDestroyBuffer( myAllocator, myMeshMap[ anIndex ].myVertexBuffer.myBuffer, myMeshMap[ anIndex ].myVertexBuffer.myAllocation );
        vmaDestroyBuffer( myAllocator, myMeshMap[ anIndex ].myIndexBuffer.myBuffer, myMeshMap[ anIndex ].myIndexBuffer.myAllocation );
    } );

    return &myMeshMap[ anIndex ];
}

Mesh* Vk_Core::GetMesh( const uint32_t anIndex ) {
    auto it = myMeshMap.find( anIndex );
    if ( it == myMeshMap.end() ) {
        return nullptr;
    }
    else {
        return &( *it ).second;
    }
}

Texture* Vk_Core::CreateTexture( const std::string& aFilename, const uint32_t anIndex ) {
    Texture tempTexture;

    if ( UtilLoader::LoadTexture( aFilename, tempTexture, myTextureImageMipLevels ) != true ) {
        throw std::runtime_error( "failed to load texture!" );
    }
    tempTexture.ImageView() = CreateImageView( tempTexture.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, myTextureImageMipLevels );

    myTextureMap[ anIndex ] = tempTexture;

    // Deletion Queue
    myDeletionQueue.pushFunction( [ = ]() {
        vmaDestroyImage( myAllocator, myTextureMap[ anIndex ].Image(), myTextureMap[ anIndex ].Allocation() );
        vkDestroyImageView( myDevice, myTextureMap[ anIndex ].ImageView(), nullptr );
    } );

    return &myTextureMap[ anIndex ];
}

Texture* Vk_Core::GetTexture( const uint32_t anIndex ) {
    auto it = myTextureMap.find( anIndex );
    if ( it == myTextureMap.end() ) {
        return nullptr;
    }
    else {
        return &( *it ).second;
    }
}

#pragma endregion

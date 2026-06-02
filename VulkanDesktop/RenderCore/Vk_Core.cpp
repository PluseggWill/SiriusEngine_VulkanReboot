// Module: Vk_Core — Vulkan device/swapchain, frame orchestration (acquire → prep → record → present).
// Draw-list CPU build: Gfx_FrameDrawStream + Vk_FrameDrawPrep; demo transforms: Gfx_DemoSceneSim (Application).
#include "Vk_Core.h"
#include "../App/DebugUIState.h"
#include "../App/WorldState.h"
#include "../Gfx/Gfx_SceneApply.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Util/Util_CameraPanel.h"
#include "../Util/Util_DebugMessenger.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_LightingPanel.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_PerfLog.h"
#include "../Util/Util_RenderDebugPanel.h"
#include "../Util/Util_ScenePanel.h"
#include "../Util/Util_StatsOverlay.h"
#include "../Util/Util_ValidationLayers.h"
#include "../Util/Util_VulkanResult.h"
#include "Vk_Bindless.h"
#include "Vk_DescriptorSystem.h"
#include "Vk_DevicePipelineCache.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_GfxPipelineCache.h"
#include "Vk_PipelineDiagnostics.h"
#include "Vk_PlatformFrame.h"
#include "Vk_RenderDevice.h"
#include "Vk_SceneHost.h"
#include "Vk_ScenePasses.h"
#include "Vk_SwapchainHost.h"

#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_RenderDoc.h"
#include "Vk_Types.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <imgui.h>
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

// Set from loaded scene before CreateGfxPipeline (repo-relative paths).
std::string vertShaderPath;
std::string fragShaderPath;
std::string bindlessFragShaderPath = "VulkanDesktop/Shader_Generated/TrianglePix_Bindless.spv";
Vk_Core::Vk_Core() {}
Vk_Core::~Vk_Core() {}

Vk_Core& Vk_Core::GetInstance() {
    static Vk_Core myInstance;
    return myInstance;
}

// Called once from Application before scene load / main loop (non-owning; must outlive Vk_Core).
void Vk_Core::BindWorldState( WorldState* aWorld ) {
    myWorld = aWorld;
}

void Vk_Core::BindDebugUI( DebugUIState* aDebugUI ) {
    myDebugUI = aDebugUI;
}

WorldState& Vk_Core::World() const {
    if ( myWorld == nullptr ) {
        throw std::runtime_error( "Vk_Core: WorldState not bound (call BindWorldState from Application)" );
    }
    return *myWorld;
}

const std::string& Vk_Core::GetLoadedSceneLogicalPath() const {
    return World().myLogicalPath;
}

bool Vk_Core::HasLoadedScene() const {
    return World().myHasLoadedScene;
}

DebugUIState& Vk_Core::DebugUI() const {
    if ( myDebugUI == nullptr ) {
        throw std::runtime_error( "Vk_Core: DebugUIState not bound (call BindDebugUI from Application)" );
    }
    return *myDebugUI;
}

void Vk_Core::SetSize( const uint32_t aWidth, const uint32_t aHeight ) {
    myWidth  = aWidth;
    myHeight = aHeight;
}

void Vk_Core::SetVsync( bool aVsync ) {
    myVsync = aVsync;
}

void Vk_Core::Reset() {
    Clear();
}

bool Vk_Core::ShouldClose() const {
    return myWindow != nullptr && glfwWindowShouldClose( myWindow );
}

void Vk_Core::ConfigureRenderDoc( bool aEnableRenderDoc ) {
    myRenderDoc.Configure( aEnableRenderDoc );
}

bool Vk_Core::IsRenderDocEnabled() const {
    return myRenderDoc.IsEnabled();
}

void Vk_Core::TriggerRenderDocCapture() {
    myRenderDoc.TriggerCaptureRequest();
}

void Vk_Core::Shutdown() {
    if ( myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myDevice );
    }
    Clear();
}

// Delegates platform/window bootstrap to Vk_PlatformFrame to keep Core focused on render orchestration.
void Vk_Core::InitWindow() {
    Vk_PlatformFrame::InitWindow( *this );
}

void Vk_Core::Clear() {
    UtilLogger::Info( "CORE", "Releasing Vulkan resources." );
    // Destruction order matters: child GPU objects -> device -> surface/debug -> instance -> window.

    ShutdownImGui();

    mySwapChainDeletionQueue.flush();
    mySceneDeletionQueue.flush();
    myDeletionQueue.flush();
    myResourceTables.Clear();

    vkDestroyCommandPool( myDevice, myGraphicsCommandPool, nullptr );
    vkDestroyCommandPool( myDevice, myTransferCommandPool, nullptr );

    // Persist pipeline cache blob while device is still valid.
    Vk_DevicePipelineCache::Destroy( *this );

    vkDestroyDevice( myDevice, nullptr );

    vkDestroySurfaceKHR( myInstance, mySurface, nullptr );

    UtilDebugMessenger::Destroy( myInstance );
    vkDestroyInstance( myInstance, nullptr );

    glfwDestroyWindow( myWindow );

    glfwTerminate();
    myRenderDoc.Shutdown();
    UtilLogger::Info( "CORE", "Resource cleanup completed." );
}

// Render device bootstrap entry point (instance/device/queues/swapchain host orchestration).
void Vk_Core::InitRenderDevice() {
    UtilLogger::Info( "VULKAN", "InitRenderDevice: instance, device, swapchain (no scene resources)." );
    // RenderDoc in-app API should be discovered before Vulkan instance/device initialization.
    myRenderDoc.InitRuntime();
    Vk_RenderDevice::Init( *this );
    myRenderDoc.BindVulkanHandles( myDevice );

    Vk_SwapchainHost::Init( *this );

    CreateFrameData();
    CreateInstanceSlabs();
    CreateUniformBuffers();
    Vk_DescriptorSystem::InitDeviceLayouts( *this );
    UtilLogger::Info( "VULKAN", "InitRenderDevice completed." );
}

// Scene-load orchestration: scene description -> CPU scene state -> pipelines -> GPU resource tables -> descriptors.
void Vk_Core::LoadSceneResources( Gfx_SceneDesc aScene, std::string aLogicalScenePath ) {
    if ( World().myHasLoadedScene ) {
        throw std::runtime_error( "LoadSceneResources: scene already loaded; call UnloadScene first." );
    }

    UtilLogger::Info( "SCENE", "LoadSceneResources." );
    World().myLoadedScene    = std::move( aScene );
    World().myLogicalPath    = std::move( aLogicalScenePath );
    World().myHasLoadedScene = true;

    DebugUI().myScenePanel.myCurrentScenePath = World().myLogicalPath;
    UtilScenePanel::RefreshSceneList( DebugUI().myScenePanel );

    ( void )Gfx_GetSceneShader( World().myLoadedScene, "lit" );  // Scene JSON contract; SPIR-V paths come from active permutation registry.
    const Gfx_ShaderPermutationDef& activePerm = Gfx_ShaderPermutation::GetActiveDefinition();
    vertShaderPath                             = activePerm.myVertSpvLogicalPath;
    fragShaderPath                             = activePerm.myFragSpvLogicalPath;
    Vk_SceneHost::LoadCpuState( World(), *this );

    Vk_GfxPipelineCache::InitScenePipelines( *this );

    {
        Gfx_ResourceManifest manifest{};
        Gfx_BuildResourceManifestFromSceneDesc( World().myLoadedScene, World().mySceneIdTables, manifest );
        myTextureImageMipLevels           = 1;
        const VkPipeline       opaquePipe = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myBasicPipelineBindless : myBasicPipeline;
        const VkPipeline       transPipe  = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myTransparentPipelineBindless : myTransparentPipeline;
        const VkPipelineLayout layout     = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myBindlessPipelineLayout : myPipelineLayout;
        SyncResourceContext();
        myResourceTables.LoadFromManifest( manifest, myResourceContext, mySceneDeletionQueue, myTextureImageMipLevels, opaquePipe, transPipe, layout );
    }

    Vk_DescriptorSystem::InitSceneDescriptors( *this );
    Gfx_SetMaterialTableGenerationForExtract( myResourceTables.GetMaterialTableGeneration() );
    Vk_SceneHost::InitScenePresentation( *this );
    InitImGui();
    UtilLogger::Info( "SCENE", "LoadSceneResources completed." );
}

void Vk_Core::UnloadScene() {
    if ( !World().myHasLoadedScene ) {
        UtilLogger::Info( "SCENE", "UnloadScene: no scene loaded (skipped)." );
        return;
    }

    UtilLogger::Info( "SCENE", "UnloadScene: releasing scene CPU + GPU resources." );
    if ( myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myDevice );
    }

    ShutdownImGui();
    Vk_GfxPipelineCache::DestroyScenePipelines( *this );
    mySceneDeletionQueue.flush();
    myResourceTables.Clear();

    myDescriptorPool        = VK_NULL_HANDLE;
    myTextureSampler        = VK_NULL_HANDLE;
    myBindlessDescriptorSet = VK_NULL_HANDLE;
    myMaterialTableBuffer   = {};
    myMaterialDescriptorSets.clear();
    myMaterialParamBuffers.clear();
    for ( Vk_FrameData& frame : myFrameDatas ) {
        frame.myGlobalDescriptors.fill( VK_NULL_HANDLE );
        frame.myObjectDescriptor = VK_NULL_HANDLE;
    }

    myDrawPrep.ClearFrameOutputs();
    myDrawPrep.ResetLogState();
    Gfx_SetMaterialTableGenerationForExtract( 0 );

    World().ClearCpuSceneState();
    DebugUI().myScenePanel.myCurrentScenePath.clear();

    myMaterialBindLoggedOnce = false;
    myBindlessLoggedOnce     = false;
    myM1PerfLoggedOnce       = false;

    UtilLogger::Info( "SCENE", "UnloadScene: GPU scene resources released." );
}

void Vk_Core::InitImGui() {
    const uint32_t imageCount    = static_cast< uint32_t >( mySwapChainImageViews.size() );
    const uint32_t minImageCount = std::max( 2u, imageCount );

    myImGuiLayer.Init( myWindow, myInstance, myPhysicalDevice, myDevice, myQueueFamilyIndices.myGraphicsFamily.value(), myGraphicsQueue, mySwapChainImageFormat,
                       mySwapChainExtent, mySwapChainImageViews, imageCount, minImageCount );
    UtilLogger::Info( "IMGUI", "ImGui overlay initialized." );
}

void Vk_Core::ShutdownImGui() {
    myImGuiLayer.Shutdown();
    UtilLogger::Info( "IMGUI", "ImGui overlay shut down." );
}

void Vk_Core::CreateInstance() {
    UtilValidationLayers::LogInstanceLayerDiscovery();

    if ( myEnableValidationLayers ) {
        UtilLogger::Info( "VULKAN", "Validation layers: enabled" );
        if ( !CheckValidationLayerSupport() ) {
            UtilLogger::Warn( "VULKAN", "Validation layers requested but unavailable. Continuing with validation disabled." );
            myEnableValidationLayers = false;
        }
    }
    else {
        UtilLogger::Info( "VULKAN", "Validation layers: disabled" );
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

    std::vector< const char* > instanceExtensions;
    instanceExtensions.reserve( glfwExtensionCount + 1u );
    for ( uint32_t i = 0; i < glfwExtensionCount; ++i ) {
        instanceExtensions.push_back( glfwExtensions[ i ] );
    }
    if ( ( myEnableValidationLayers || myRenderDoc.WantsDebugUtilsExtension() ) && UtilDebugMessenger::IsExtensionAvailable() ) {
        instanceExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( instanceExtensions.size() );
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if ( myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myValidationLayers.data();
        UtilDebugMessenger::SetupForInstanceCreate( createInfo );
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

#ifdef _DEBUG
    CheckExtensionSupport();
#endif  // _DEBUG

    if ( vkCreateInstance( &createInfo, nullptr, &myInstance ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateInstance failed." );
        throw std::runtime_error( "failed to create instance!" );
    }
    UtilLogger::Info( "VULKAN", "Vulkan instance created." );

    if ( myEnableValidationLayers ) {
        UtilDebugMessenger::Create( myInstance );
    }
}

void Vk_Core::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, nullptr );

    if ( deviceCount == 0 )
        throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

    std::vector< VkPhysicalDevice > devices( deviceCount );
    vkEnumeratePhysicalDevices( myInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices ) {
        if ( CheckDeviceSuitable( device ) ) {
            myPhysicalDevice = device;
            // Keep startup stable across GPUs/drivers first; revisit dynamic MSAA selection later.
            myMSAASamples = VK_SAMPLE_COUNT_1_BIT;
            break;
        }
    }

    if ( myPhysicalDevice == VK_NULL_HANDLE )
        throw std::runtime_error( "failed to find a suitable GPU!" );

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties( myPhysicalDevice, &props );
    UtilLogger::Info( "GPU", std::string( "Selected physical device: " ) + props.deviceName );
}

void Vk_Core::CreateLogicalDevice() {
    // Build one queue create-info per unique family (graphics/present/transfer may collapse to fewer families).
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

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fillModeNonSolid  = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    if ( myBindlessCaps.myDescriptorIndexingExtension ) {
        indexingFeatures.runtimeDescriptorArray                    = VK_TRUE;
        indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features = deviceFeatures;
    features2.pNext    = myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &indexingFeatures ) : nullptr;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &features2 ) : nullptr;

    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.queueCreateInfoCount    = static_cast< uint32_t >( queueCreateInfos.size() );
    createInfo.pEnabledFeatures        = myBindlessCaps.myDescriptorIndexingExtension ? nullptr : &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( myDeviceExtensions.size() );
    createInfo.ppEnabledExtensionNames = myDeviceExtensions.data();

    if ( myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myValidationLayers.data();
    }
    else
        createInfo.enabledLayerCount = 0;

    if ( vkCreateDevice( myPhysicalDevice, &createInfo, nullptr, &myDevice ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateDevice failed." );
        throw std::runtime_error( "failed to create logical device!" );
    }
    UtilLogger::Info( "VULKAN", "Logical device created." );

    // Resolve queue handles after logical-device creation.
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myGraphicsFamily.value(), 0, &myGraphicsQueue );
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myPresentFamily.value(), 0, &myPresentQueue );
    vkGetDeviceQueue( myDevice, myQueueFamilyIndices.myTransferFamily.value(), 0, &myTransferQueue );
}

void Vk_Core::CreateSurface() {
    if ( glfwCreateWindowSurface( myInstance, myWindow, nullptr, &mySurface ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "glfwCreateWindowSurface failed." );
        throw std::runtime_error( "failed to create window surface!" );
    }
    UtilLogger::Info( "VULKAN", "Window surface created." );
}

void Vk_Core::SyncResourceContext() {
    myResourceContext.Bind( myDevice, myAllocator, myPhysicalDevice, myGraphicsQueue, myTransferQueue, myGraphicsCommandPool, myTransferCommandPool,
                            myQueueFamilyIndices.myGraphicsFamily.value_or( 0 ), myQueueFamilyIndices.myTransferFamily.value_or( 0 ) );
}

void Vk_Core::InitAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = myPhysicalDevice;
    allocatorInfo.device         = myDevice;
    allocatorInfo.instance       = myInstance;
    vmaCreateAllocator( &allocatorInfo, &myAllocator );
    SyncResourceContext();

    // Lifetime bound to core deletion queue (survives swapchain recreate).
    myDeletionQueue.pushFunction( [ = ]() { vmaDestroyAllocator( myAllocator ); } );
}

void Vk_Core::CreateFrameData() {
    myFrameDatas.resize( MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        // Per-frame command buffer.
        const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( myGraphicsCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );

        if ( vkAllocateCommandBuffers( myDevice, &allocInfo, &myFrameDatas[ i ].myCommandBuffer ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate command buffers!" );
        }

        // Per-frame camera UBO slab (one slot per render view).
        VkDeviceSize bufferSize = static_cast< VkDeviceSize >( kGfxMaxRenderViews ) * static_cast< VkDeviceSize >( PadUniformBufferSize( sizeof( GpuCameraData ) ) );

        CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameDatas[ i ].myCameraBuffer, true );

        // Per-frame sync primitives (fence starts signaled for first frame).
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

        // Core-owned lifetime cleanup.
        myDeletionQueue.pushFunction( [ = ]() {
            vmaDestroyBuffer( myAllocator, myFrameDatas[ i ].myCameraBuffer.myBuffer, myFrameDatas[ i ].myCameraBuffer.myAllocation );
            vkDestroySemaphore( myDevice, myFrameDatas[ i ].myPresentSemaphore, nullptr );
            vkDestroySemaphore( myDevice, myFrameDatas[ i ].myRenderSemaphore, nullptr );
            vkDestroyFence( myDevice, myFrameDatas[ i ].myRenderFence, nullptr );
        } );
    }
}

void Vk_Core::CreateInstanceSlabs() {
    const VkDeviceSize slabSize = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxInstanceSlabEntries ) * static_cast< VkDeviceSize >( InstanceSlabStride() );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( slabSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameDatas[ i ].myObjectBuffer, true );

        void* mapped = nullptr;
        vmaMapMemory( myAllocator, myFrameDatas[ i ].myObjectBuffer.myAllocation, &mapped );
        myFrameDatas[ i ].myInstanceSlabMapped = mapped;

        myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameDatas[ i ].myInstanceSlabMapped != nullptr ) {
                vmaUnmapMemory( myAllocator, myFrameDatas[ i ].myObjectBuffer.myAllocation );
                myFrameDatas[ i ].myInstanceSlabMapped = nullptr;
            }
            vmaDestroyBuffer( myAllocator, myFrameDatas[ i ].myObjectBuffer.myBuffer, myFrameDatas[ i ].myObjectBuffer.myAllocation );
        } );
    }

    UtilLogger::Info( "RESOURCE", "Instance slab: entries=" + std::to_string( VkDescriptorPolicy::kMaxInstanceSlabEntries )
                                      + " stride=" + std::to_string( InstanceSlabStride() ) + " bytes/frame=" + std::to_string( slabSize ) );
}

void Vk_Core::CreateUniformBuffers() {
    // Device-scoped env UBO slab (CPU defaults written at scene load — Vk_SceneHost::InitScenePresentation).
    // Each in-flight frame uses a static slice offset (not UNIFORM_BUFFER_DYNAMIC).
    const size_t envDataBufferSize = MAX_FRAMES_IN_FLIGHT * PadUniformBufferSize( sizeof( GpuEnvironmentData ) );
    CreateBuffer( envDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, myEnvDataBuffer, true );

    myDeletionQueue.pushFunction( [ = ]() { vmaDestroyBuffer( myAllocator, myEnvDataBuffer.myBuffer, myEnvDataBuffer.myAllocation ); } );
}

void Vk_Core::CreateCommandPool() {
    // Graphics command pool: frame command buffers + graphics-side one-shot commands.
    VkCommandPoolCreateInfo poolInfo = VkInit::CommandPoolCreateInfo( myQueueFamilyIndices.myGraphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myGraphicsCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphic command pool!" );
    }

    // Transfer command pool: staging copy path when transfer queue differs.
    poolInfo = VkInit::CommandPoolCreateInfo( myQueueFamilyIndices.myTransferFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    if ( vkCreateCommandPool( myDevice, &poolInfo, nullptr, &myTransferCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create transfer command pool!" );
    }
}

namespace {

float ElapsedMs( std::chrono::high_resolution_clock::time_point aStart, std::chrono::high_resolution_clock::time_point aEnd ) {
    return std::chrono::duration< float, std::milli >( aEnd - aStart ).count();
}

glm::vec4 ClampNormalizedViewport( const glm::vec4& aViewport ) {
    const float x = std::clamp( aViewport.x, 0.0f, 1.0f );
    const float y = std::clamp( aViewport.y, 0.0f, 1.0f );
    const float w = std::clamp( aViewport.z, 0.0f, 1.0f - x );
    const float h = std::clamp( aViewport.w, 0.0f, 1.0f - y );
    return { x, y, w, h };
}

VkViewport ToViewport( const VkExtent2D& aExtent, const glm::vec4& aViewportNorm ) {
    const float x      = aViewportNorm.x * static_cast< float >( aExtent.width );
    const float y      = aViewportNorm.y * static_cast< float >( aExtent.height );
    const float width  = std::max( 1.0f, aViewportNorm.z * static_cast< float >( aExtent.width ) );
    const float height = std::max( 1.0f, aViewportNorm.w * static_cast< float >( aExtent.height ) );
    return VkViewport{ x, y, width, height, 0.0f, 1.0f };
}

VkRect2D ToScissor( const VkExtent2D& aExtent, const VkViewport& aViewport ) {
    VkRect2D scissor{};
    scissor.offset.x = std::max( 0, static_cast< int32_t >( aViewport.x ) );
    scissor.offset.y = std::max( 0, static_cast< int32_t >( aViewport.y ) );

    const uint32_t offsetX = static_cast< uint32_t >( scissor.offset.x );
    const uint32_t offsetY = static_cast< uint32_t >( scissor.offset.y );
    const uint32_t maxWidthFromOffset  = offsetX < aExtent.width ? ( aExtent.width - offsetX ) : 1u;
    const uint32_t maxHeightFromOffset = offsetY < aExtent.height ? ( aExtent.height - offsetY ) : 1u;
    // Clamp extent relative to offset to keep the scissor rectangle inside the swapchain extent.
    scissor.extent.width  = std::max( 1u, std::min( maxWidthFromOffset, static_cast< uint32_t >( aViewport.width ) ) );
    scissor.extent.height = std::max( 1u, std::min( maxHeightFromOffset, static_cast< uint32_t >( aViewport.height ) ) );
    return scissor;
}

}  // namespace

std::array< Vk_Core::ActiveRenderView, kGfxMaxRenderViews > Vk_Core::BuildActiveRenderViews( uint32_t& aOutViewCount, const WorldState& aWorld, const DebugUIState& aDebugUI ) const {
    std::array< ActiveRenderView, kGfxMaxRenderViews > views{};
    aOutViewCount = 1;

    views[ 0 ].myView.myCameraSource = Gfx_RenderViewCameraSource::Fly;
    views[ 0 ].myView.myViewport     = glm::vec4( 0.0f, 0.0f, 1.0f, 1.0f );
    views[ 0 ].myView.myLayerMask    = 0xFFFFFFFFu;
    views[ 0 ].myCamera              = myCamera;
    views[ 0 ].myViewport            = ToViewport( mySwapChainExtent, views[ 0 ].myView.myViewport );
    views[ 0 ].myScissor             = ToScissor( mySwapChainExtent, views[ 0 ].myViewport );

    if ( aDebugUI.myMultiView.myEnablePiP && !aWorld.myLoadedScene.myCameras.empty() ) {
        aOutViewCount = 2;
        const uint32_t cameraIndex = std::min( aDebugUI.myMultiView.mySecondaryCameraIndex, static_cast< uint32_t >( aWorld.myLoadedScene.myCameras.size() - 1 ) );
        const Gfx_SceneCameraEntry& sceneCamera = aWorld.myLoadedScene.myCameras[ cameraIndex ];
        const glm::vec4 viewportNorm = ClampNormalizedViewport( sceneCamera.myViewport );

        views[ 1 ].myView.myCameraSource   = Gfx_RenderViewCameraSource::SceneCamera;
        views[ 1 ].myView.mySceneCameraIndex = cameraIndex;
        views[ 1 ].myView.myViewport       = viewportNorm;
        views[ 1 ].myView.myLayerMask      = sceneCamera.myLayerMask;
        views[ 1 ].myViewport              = ToViewport( mySwapChainExtent, viewportNorm );
        views[ 1 ].myScissor               = ToScissor( mySwapChainExtent, views[ 1 ].myViewport );

        const float aspect = static_cast< float >( views[ 1 ].myScissor.extent.width ) / static_cast< float >( views[ 1 ].myScissor.extent.height );
        views[ 1 ].myCamera.SetLens( sceneCamera.myFovYDeg, myCamera.myNear, myCamera.myFar, aspect );
        views[ 1 ].myCamera.LookAt( sceneCamera.myEye, sceneCamera.myCenter, sceneCamera.myUp );
    }

    return views;
}

bool Vk_Core::PrepareFrameCpu( WorldState& aWorld, const DebugUIState& aDebugUI, Vk_FrameCpuPrepResult& aOut ) {
    aOut = Vk_FrameCpuPrepResult{};

    myRenderDoc.BeginFrameCaptureIfRequested();

    Vk_FrameData& frameData = myFrameDatas[ myCurrentFrame ];
    aOut.myFrameData        = &frameData;

    const auto fenceWaitStart = std::chrono::high_resolution_clock::now();
    vkWaitForFences( myDevice, 1, &frameData.myRenderFence, VK_TRUE, UINT64_MAX );
    aOut.myGpuFenceWaitMs = ElapsedMs( fenceWaitStart, std::chrono::high_resolution_clock::now() );

    if ( !Vk_SwapchainHost::AcquireNextImage( *this, frameData, aOut.myImageIndex ) ) {
        return false;
    }

    myFrameStats.ResetPerFrameCounters();

    const auto activeViews = BuildActiveRenderViews( aOut.myActiveViewCount, aWorld, aDebugUI );

    uint32_t totalOpaqueDraws = 0;
    uint32_t totalTransDraws  = 0;
    uint32_t totalOpaqueRuns  = 0;
    uint32_t totalTransRuns   = 0;
    const uint32_t slabPartitionCount = std::max( 1u, aOut.myActiveViewCount );
    const uint32_t perViewMaxEntries  = std::max( 1u, VkDescriptorPolicy::kMaxInstanceSlabEntries / slabPartitionCount );

    for ( uint32_t viewIndex = 0; viewIndex < aOut.myActiveViewCount; ++viewIndex ) {
        Gfx_LodState  secondaryViewLodState;
        Gfx_LodState* lodStateForView = &aWorld.myLodState;
        if ( viewIndex > 0 ) {
            secondaryViewLodState = aWorld.myLodState;
            lodStateForView       = &secondaryViewLodState;
        }

        Vk_FrameDrawPrepBuildParams prepParams{};
        prepParams.myScene                  = &aWorld.mySceneSoA;
        prepParams.myCamera                 = &activeViews[ viewIndex ].myCamera;
        prepParams.myLodTable               = &aWorld.myLodTable;
        prepParams.myLodState               = lodStateForView;
        prepParams.myLodDebugLogicalMeshId  = aWorld.myLodDebugLogicalMeshId;
        prepParams.myCurrentFrame           = myCurrentFrame;
        prepParams.myFrameDatas             = &myFrameDatas;
        prepParams.myInstanceSlabStride     = InstanceSlabStride();
        prepParams.myInstanceSlabBaseOffset = static_cast< size_t >( viewIndex ) * static_cast< size_t >( perViewMaxEntries ) * InstanceSlabStride();
        prepParams.myInstanceSlabMaxEntries = perViewMaxEntries;

        myDrawPrep.ClearFrameOutputs();
        myDrawPrep.Build( prepParams );
        aOut.myViewPackets[ viewIndex ]      = myDrawPrep.myFramePacket;
        aOut.myViewports[ viewIndex ]        = activeViews[ viewIndex ].myViewport;
        aOut.myScissors[ viewIndex ]         = activeViews[ viewIndex ].myScissor;
        aOut.myFrameDescriptors[ viewIndex ] = myFrameDatas[ myCurrentFrame ].myGlobalDescriptors[ viewIndex ];
        totalOpaqueDraws += static_cast< uint32_t >( myDrawPrep.myFramePacket.myOpaquePass.myDraws.size() );
        totalTransDraws += static_cast< uint32_t >( myDrawPrep.myFramePacket.myTransparentPass.myDraws.size() );
        totalOpaqueRuns += static_cast< uint32_t >( myDrawPrep.myFramePacket.myOpaquePass.myBatchRuns.size() );
        totalTransRuns += static_cast< uint32_t >( myDrawPrep.myFramePacket.myTransparentPass.myBatchRuns.size() );
        Vk_FrameUniformUploader::UpdateForView( *this, myCurrentFrame, viewIndex, activeViews[ viewIndex ].myCamera );
    }

    aOut.myTotalOpaqueDraws      = totalOpaqueDraws;
    aOut.myTotalTransparentDraws = totalTransDraws;
    myFrameStats.SetDrawStreamMetrics( static_cast< uint32_t >( aWorld.mySceneSoA.GetActiveCount() ), totalOpaqueDraws, totalTransDraws, totalOpaqueRuns, totalTransRuns );

    const uint32_t visibleDrawsForPerf = totalOpaqueDraws + totalTransDraws;
    UtilPerfLog::AppendFrame( static_cast< uint64_t >( myFrameNumber ), myFrameStats.myFrameMs, myFrameStats.myDrawCalls, visibleDrawsForPerf, aOut.myActiveViewCount,
                              Vk_RenderMaterialPathName( myMaterialPath ) );

    if ( !myMaterialBindLoggedOnce ) {
        if ( myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
            UtilLogger::Info( "DESCRIPTOR", "Set 1 bindless material set (once per pass); per-draw materialIndex in Set 2 slab." );
        }
        else {
            UtilLogger::Info( "DESCRIPTOR",
                              "Set 1 material binds this frame will be <= batch runs (" + std::to_string( totalOpaqueRuns + totalTransRuns ) + ")" );
        }
        myMaterialBindLoggedOnce = true;
    }

    if ( !myBindlessLoggedOnce && myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        UtilLogger::Info( "BINDLESS", "recording with materialTableGeneration=" + std::to_string( myResourceTables.GetMaterialTableGeneration() )
                                          + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds ) );
        myBindlessLoggedOnce = true;
    }

    aOut.myOk = true;
    return true;
}

void Vk_Core::DrawFrameGpu( const DebugUIState& aDebugUI, Vk_FrameCpuPrepResult& aPrep ) {
    ( void )aDebugUI;
    if ( !aPrep.myOk || aPrep.myFrameData == nullptr ) {
        return;
    }

    Vk_FrameData& frameData = *aPrep.myFrameData;

    // Env UBO upload after Application built debug panels (RenderDebug must precede this).
    Vk_FrameUniformUploader::Update( *this, myCurrentFrame );

    vkResetFences( myDevice, 1, &frameData.myRenderFence );
    vkResetCommandBuffer( frameData.myCommandBuffer, 0 );

    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( 0 );
    if ( vkBeginCommandBuffer( frameData.myCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to begin recording command buffer!" );
    }

    Vk_ScenePasses::RecordScene( *this, frameData.myCommandBuffer, aPrep.myImageIndex, aPrep.myViewports, aPrep.myScissors, aPrep.myFrameDescriptors, aPrep.myActiveViewCount,
                                 aPrep.myViewPackets );

    // Lighting tuning applies next frame (unchanged contract).
    UtilLightingPanel::Build( myEnvironmentData );

    ImGui::Render();
    Vk_ScenePasses::RecordImGui( *this, frameData.myCommandBuffer, aPrep.myImageIndex );

    if ( vkEndCommandBuffer( frameData.myCommandBuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to record command buffer!" );
    }

    Vk_SwapchainHost::SubmitAndPresent( *this, frameData, aPrep.myImageIndex );

    if ( myHasFrameInputSampleTime ) {
        const float inputToPresentMs = ElapsedMs( myFrameInputSampleTime, std::chrono::high_resolution_clock::now() );
        myFrameStats.RecordInputLatency( inputToPresentMs, aPrep.myGpuFenceWaitMs, myVsync, myFrameStats.myFrameMs );
        myHasFrameInputSampleTime = false;
    }

    if ( !myM1PerfLoggedOnce && myFrameNumber >= 59 ) {
        LogM1PerfSnapshot();
        myM1PerfLoggedOnce = true;
    }

    myFrameNumber++;
    myCurrentFrame = myFrameNumber % MAX_FRAMES_IN_FLIGHT;
}

void Vk_Core::CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const {
    myRenderDoc.CmdBeginDebugLabel( aCommandBuffer, aLabelName );
}

void Vk_Core::CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const {
    myRenderDoc.CmdEndDebugLabel( aCommandBuffer );
}

void Vk_Core::LogM1PerfSnapshot() const {
    const uint32_t visibleDraws = myFrameStats.myVisibleOpaqueDraws + myFrameStats.myVisibleTransparentDraws;
    const uint32_t batchRuns    = myFrameStats.myOpaqueBatchRuns + myFrameStats.myTransparentBatchRuns;
    UtilLogger::Info( "PERF", "frameMs=" + std::to_string( myFrameStats.myFrameMs ) + " fps=" + std::to_string( myFrameStats.myFps )
                                  + " entities=" + std::to_string( myFrameStats.myActiveEntities ) + " visibleDraws=" + std::to_string( visibleDraws )
                                  + " batchRuns=" + std::to_string( batchRuns ) + " (opaque=" + std::to_string( myFrameStats.myOpaqueBatchRuns ) + " transparent="
                                  + std::to_string( myFrameStats.myTransparentBatchRuns ) + ")" + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds )
                                  + " pipelineBinds=" + std::to_string( myFrameStats.myPipelineBinds ) + " drawCalls=" + std::to_string( myFrameStats.myDrawCalls )
                                  + " materialPath=" + Vk_RenderMaterialPathName( myMaterialPath ) );
}

void Vk_Core::RefreshMaterialPipelinesAfterSwapchainRecreate() {
    const VkPipeline       opaquePipe = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myBasicPipelineBindless : myBasicPipeline;
    const VkPipeline       transPipe  = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myTransparentPipelineBindless : myTransparentPipeline;
    const VkPipelineLayout layout     = myMaterialPath == Vk_RenderMaterialPath::Bindless ? myBindlessPipelineLayout : myPipelineLayout;
    myResourceTables.RefreshMaterialPipelines( opaquePipe, transPipe, layout );
}

void Vk_Core::InitVk_QueueFamilyIndices() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( myPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( myPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( myPhysicalDevice, i, mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myQueueFamilyIndices.myPresentFamily  = i;
            myQueueFamilyIndices.myGraphicsFamily = i;
        }

        // Prefer a dedicated transfer-only family when available (staging uploads).
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myQueueFamilyIndices.myTransferFamily = i;
        }

        if ( myQueueFamilyIndices.isComplete() )
            break;

        i++;
    }

    myQueueFamilyIndices.ApplyTransferFallback();

    const uint32_t graphicsFamily = myQueueFamilyIndices.myGraphicsFamily.value_or( 0 );
    const uint32_t transferFamily = myQueueFamilyIndices.myTransferFamily.value_or( graphicsFamily );
    const bool     useConcurrent  = graphicsFamily != transferFamily;
    // Startup signal for queue-family ownership policy used by image/buffer allocations.
    UtilLogger::Info( "VULKAN", "Queue families: graphics=" + std::to_string( graphicsFamily ) + " transfer=" + std::to_string( transferFamily )
                                    + " (image/buffer sharing=" + std::string( useConcurrent ? "CONCURRENT" : "EXCLUSIVE" ) + ")" );
}

#pragma region Helpers - device, queues, shaders

void Vk_Core::CheckExtensionSupport() const {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > extensions( extensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );

    UtilLogger::Info( "VULKAN", "Instance extension discovery (" + std::to_string( extensionCount ) + "):" );
    for ( const VkExtensionProperties& extension : extensions ) {
        UtilLogger::Info( "VULKAN", std::string( "  " ) + extension.extensionName );
    }
}

bool Vk_Core::CheckValidationLayerSupport() const {
    return UtilValidationLayers::AreLayersAvailable( myValidationLayers );
}

void Vk_Core::SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers ) {
    myEnableValidationLayers = aEnableValidationLayers;
    myValidationLayers       = someValidationLayers;
}

void Vk_Core::SetRequiredExtension( std::vector< const char* > someDeviceExtensions ) {
    myDeviceExtensions = someDeviceExtensions;
}

bool Vk_Core::CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const {
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &myPhysicalDeviceProperties );

    vkGetPhysicalDeviceFeatures( aPhysicalDevice, &myPhysicalDeviceFeatures );

    Vk_QueueFamilyIndices indices = FindQueueFamilies( aPhysicalDevice );

    bool extensionSupported = CheckExtensionSupport( aPhysicalDevice );

    // Swapchain formats + present modes are required for renderable output.
    bool swapChainAdequate = false;
    if ( extensionSupported ) {
        Vk_SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( aPhysicalDevice );
        swapChainAdequate                           = !swapChainSupport.myFormats.empty() && !swapChainSupport.myPresentModes.empty();
    }

#ifdef _DEBUG
    UtilLogger::Debug( "GPU", "minUniformBufferOffsetAlignment=" + std::to_string( myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment ) );
#endif  // _DEBUG

    return indices.isComplete() && extensionSupported && swapChainAdequate && myPhysicalDeviceFeatures.samplerAnisotropy;
}

Vk_QueueFamilyIndices Vk_Core::FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const {
    Vk_QueueFamilyIndices indices;
    uint32_t              queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        // Prefer one family that supports both graphics + present.
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( aPhysicalDevice, i, mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            indices.myPresentFamily  = i;
            indices.myGraphicsFamily = i;
        }

        // Prefer dedicated transfer-only family for staging.
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            indices.myTransferFamily = i;
        }

        if ( indices.isComplete() )
            break;

        i++;
    }

    indices.ApplyTransferFallback();
    return indices;
}

Vk_SwapChainSupportDetails Vk_Core::QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const {
    Vk_SwapChainSupportDetails details;

    // Surface capabilities (extent/image count transform limits).
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( aPhysicalDevice, mySurface, &details.myCapabilities );

    // Supported color formats/colorspaces for present images.
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, mySurface, &formatCount, nullptr );

    if ( formatCount != 0 ) {
        details.myFormats.resize( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, mySurface, &formatCount, details.myFormats.data() );
    }

    // Supported present modes (FIFO/MAILBOX/IMMEDIATE).
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

    if ( !requiredExtensions.empty() ) {
        std::string missing;
        for ( const std::string& name : requiredExtensions ) {
            if ( !missing.empty() ) {
                missing += ", ";
            }
            missing += name;
        }
        UtilLogger::Warn( "VULKAN", "Device missing required extensions: " + missing );
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
    auto hasMode = [ & ]( VkPresentModeKHR aMode ) {
        return std::find( someAvailablePresentModes.begin(), someAvailablePresentModes.end(), aMode ) != someAvailablePresentModes.end();
    };

    if ( myVsync ) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    if ( hasMode( VK_PRESENT_MODE_MAILBOX_KHR ) ) {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if ( hasMode( VK_PRESENT_MODE_IMMEDIATE_KHR ) ) {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
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
    UtilVulkanResult::ThrowOnFailure( vkCreateShaderModule( myDevice, &createInfo, nullptr, &shaderModule ), "vkCreateShaderModule" );

    return shaderModule;
}

VkShaderModule Vk_Core::CreateShaderModule( const std::string aShaderPath ) const {
    UtilLogger::Info( "SHADER", "Loading shader module: " + aShaderPath );
    const auto shaderCode = UtilLoader::ReadFile( aShaderPath );

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( shaderCode.data() );

    VkShaderModule shaderModule;
    const VkResult moduleResult = vkCreateShaderModule( myDevice, &createInfo, nullptr, &shaderModule );
    if ( moduleResult != VK_SUCCESS ) {
        UtilLogger::Error( "SHADER", "vkCreateShaderModule " + UtilVulkanResult::Describe( moduleResult ) + " path=" + aShaderPath
                                         + " codeSize=" + std::to_string( shaderCode.size() ) );
    }
    UtilVulkanResult::ThrowOnFailure( moduleResult, "vkCreateShaderModule" );

    return shaderModule;
}

// Required when bound pipeline declares dynamic viewport/scissor/line width (SetDefaultDynamicStates).
void Vk_Core::SetGraphicsDynamicState( VkCommandBuffer aCommandBuffer, const VkViewport& aViewport, const VkRect2D& aScissor ) const {
    // CONTRACT: VkDynamicState list must match Vk_PipelineBuilder::SetDefaultDynamicStates().
    vkCmdSetViewport( aCommandBuffer, 0, 1, &aViewport );
    vkCmdSetScissor( aCommandBuffer, 0, 1, &aScissor );

    vkCmdSetLineWidth( aCommandBuffer, 1.0f );
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

void Vk_Core::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool isExclusive ) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = aSize;
    bufferInfo.usage = aBufferUsage;

    if ( isExclusive ) {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else {
        const uint32_t graphicsFamily = myQueueFamilyIndices.myGraphicsFamily.value();
        const uint32_t transferFamily = myQueueFamilyIndices.myTransferFamily.value();

        // Same family after ApplyTransferFallback: CONCURRENT with duplicate indices is invalid.
        if ( graphicsFamily == transferFamily ) {
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else {
            const uint32_t queueFamilyIndices[] = { graphicsFamily, transferFamily };

            bufferInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
            bufferInfo.queueFamilyIndexCount = 2;
            bufferInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
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

size_t Vk_Core::InstanceSlabStride() const {
    return PadUniformBufferSize( sizeof( GpuObjectData ) );
}

// Per-frame UBO upload is delegated to Vk_FrameUniformUploader.
void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                           Vk_AllocatedImage& anImage ) const {
    CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, 1, VK_SAMPLE_COUNT_1_BIT, anImage );
}

void Vk_Core::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    VkExtent3D extent = { anExtent.width, anExtent.height, 1 };

    CreateImage( extent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    VkImageCreateInfo imageInfo = VkInit::ImageCreateInfo( aFormat, anImageUsage, anExtent );

    imageInfo.mipLevels = aMipLevel;
    imageInfo.tiling    = aTiling;
    imageInfo.samples   = aNumSamples;
    imageInfo.flags     = 0;
    std::array< uint32_t, 2 > queueFamilyIndices{};
    VkInit::FillImageSharingMode( myQueueFamilyIndices.myGraphicsFamily.value(), myQueueFamilyIndices.myTransferFamily.value(), queueFamilyIndices, imageInfo );

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
    return myResourceContext.CreateImageView( anImage, aFormat, anAspect, aMipLevel );
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

// Platform tick and ImGui NewFrame bootstrap are delegated to Vk_PlatformFrame.
void Vk_Core::BeginPlatformFrame( float& aOutDeltaSeconds ) {
    Vk_PlatformFrame::BeginFrame( *this, aOutDeltaSeconds );
}

void Vk_Core::ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput ) {
    myCamera.ApplyInput( aDeltaSeconds, aInput, DebugUI().myCameraSettings );
}

void Vk_Core::SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime ) {
    myFrameInputSampleTime    = aSampleTime;
    myHasFrameInputSampleTime = true;
}

size_t Vk_Core::PadUniformBufferSize( size_t anOriginalSize ) const {
    // Slab stride / future UNIFORM_BUFFER_DYNAMIC offsets - multiples of minUniformBufferOffsetAlignment.
    size_t minUboAlignment = myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize     = anOriginalSize;

    if ( minUboAlignment > 0 )
        alignedSize = ( alignedSize + minUboAlignment - 1 ) & ~( minUboAlignment - 1 );

    return alignedSize;
}

#pragma endregion

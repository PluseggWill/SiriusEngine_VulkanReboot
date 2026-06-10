// Module: Vk_Core ? Vulkan device/swapchain, frame orchestration (acquire ? prep ? record ? present).
// Draw-list CPU build: Gfx_FrameDrawStream + Vk_FrameDrawPrep; demo transforms: Gfx_DemoSceneSim (Application).
#include "Vk_Core.h"
#include "../App/DebugUIState.h"
#include "../App/WorldState.h"
#include "../Gfx/Gfx_DrawTemplate.h"
#include "../Gfx/Gfx_EntityGpuRecord.h"
#include "../Gfx/Gfx_GpuCull.h"
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
#include "Vk_GpuCull.h"
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
    static Vk_Core sInstance;
    return sInstance;
}

// Called once from Application before scene load / main loop (non-owning; must outlive Vk_Core).
void Vk_Core::BindWorldState( WorldState* aWorld ) {
    myWorld = aWorld;
}

void Vk_Core::BindDebugUI( DebugUIState* aDebugUI ) {
    myDebugUI = aDebugUI;
}

void Vk_Core::BindEngineConfig( const Util_EngineConfig* aConfig ) {
    myEngineConfig = aConfig;
}

const Util_EngineConfig& Vk_Core::EngineConfig() const {
    if ( myEngineConfig == nullptr ) {
        throw std::runtime_error( "Vk_Core: Util_EngineConfig not bound (call BindEngineConfig from Application)" );
    }
    return *myEngineConfig;
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
    myPlatformCtx.myWidth  = aWidth;
    myPlatformCtx.myHeight = aHeight;
}

void Vk_Core::SetVsync( bool aVsync ) {
    myVsync = aVsync;
}

void Vk_Core::Reset() {
    Clear();
}

bool Vk_Core::ShouldClose() const {
    return myPlatformCtx.myWindow != nullptr && glfwWindowShouldClose( myPlatformCtx.myWindow );
}

void Vk_Core::ConfigureRenderDoc( bool aEnableRenderDoc ) {
    myPlatformCtx.myRenderDoc.Configure( aEnableRenderDoc );
}

bool Vk_Core::IsRenderDocEnabled() const {
    return myPlatformCtx.myRenderDoc.IsEnabled();
}

void Vk_Core::TriggerRenderDocCapture() {
    myPlatformCtx.myRenderDoc.TriggerCaptureRequest();
}

void Vk_Core::Shutdown() {
    if ( myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myDeviceCtx.myDevice );
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

    mySwapchainCtx.mySwapChainDeletionQueue.flush();
    mySceneGpuCtx.mySceneDeletionQueue.flush();
    myDeviceCtx.myDeletionQueue.flush();
    mySceneGpuCtx.myResourceTables.Clear();

    vkDestroyCommandPool( myDeviceCtx.myDevice, myDeviceCtx.myGraphicsCommandPool, nullptr );
    vkDestroyCommandPool( myDeviceCtx.myDevice, myDeviceCtx.myTransferCommandPool, nullptr );

    // Persist pipeline cache blob while device is still valid.
    Vk_DevicePipelineCache::Destroy( *this );

    vkDestroyDevice( myDeviceCtx.myDevice, nullptr );

    vkDestroySurfaceKHR( myDeviceCtx.myInstance, myDeviceCtx.mySurface, nullptr );

    UtilDebugMessenger::Destroy( myDeviceCtx.myInstance );
    vkDestroyInstance( myDeviceCtx.myInstance, nullptr );

    glfwDestroyWindow( myPlatformCtx.myWindow );

    glfwTerminate();
    myPlatformCtx.myRenderDoc.Shutdown();
    UtilLogger::Info( "CORE", "Resource cleanup completed." );
}

// Render device bootstrap entry point (instance/device/queues/swapchain host orchestration).
void Vk_Core::InitRenderDevice() {
    UtilLogger::Info( "VULKAN", "InitRenderDevice: instance, device, swapchain (no scene resources)." );
    // RenderDoc in-app API should be discovered before Vulkan instance/device initialization.
    myPlatformCtx.myRenderDoc.InitRuntime();
    Vk_RenderDevice::Init( *this );
    myPlatformCtx.myRenderDoc.BindVulkanHandles( myDeviceCtx.myDevice );

    Vk_SwapchainHost::Init( *this );

    CreateFrameData();
    CreateInstanceSlabs();
    CreateDrawTemplateBuffers();
    CreateEntityRecordBuffers();
    Vk_GpuCull::Init( *this );
    Vk_GpuCull::CreateFrameBuffers( *this );
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
    UtilScenePanel::RefreshSceneList( EngineConfig(), DebugUI().myScenePanel );

    ( void )Gfx_GetSceneShader( World().myLoadedScene, "lit" );  // Scene JSON contract; SPIR-V paths come from active permutation registry.
    const Gfx_ShaderPermutationDef& activePerm = Gfx_ShaderPermutation::GetActiveDefinition();
    vertShaderPath                             = activePerm.myVertSpvLogicalPath;
    fragShaderPath                             = activePerm.myFragSpvLogicalPath;
    Vk_SceneHost::LoadCpuState( World(), *this );

    Vk_GfxPipelineCache::InitScenePipelines( *this );

    {
        Gfx_ResourceManifest manifest{};
        Gfx_BuildResourceManifestFromSceneDesc( World().myLoadedScene, World().mySceneIdTables, manifest );
        mySceneGpuCtx.myTextureImageMipLevels = 1;
        const VkPipeline opaquePipe = myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBasicPipelineBindless : mySceneGpuCtx.myBasicPipeline;
        const VkPipeline transPipe =
            myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myTransparentPipelineBindless : mySceneGpuCtx.myTransparentPipeline;
        const VkPipelineLayout layout =
            myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBindlessPipelineLayout : mySceneGpuCtx.myPipelineLayout;
        SyncResourceContext();
        mySceneGpuCtx.myResourceTables.LoadFromManifest( EngineConfig(), manifest, myResourceContext, mySceneGpuCtx.mySceneDeletionQueue,
                                                         mySceneGpuCtx.myTextureImageMipLevels, opaquePipe, transPipe, layout );
        Gfx_ApplyMeshLocalBoundsToSceneSoA( World().myLoadedScene, World().mySceneIdTables, mySceneGpuCtx.myResourceTables.CollectMeshLocalBounds(), World().mySceneSoA );
    }

    Vk_DescriptorSystem::InitSceneDescriptors( *this );
    Gfx_SetMaterialTableGenerationForExtract( mySceneGpuCtx.myResourceTables.GetMaterialTableGeneration() );
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
    if ( myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myDeviceCtx.myDevice );
    }

    ShutdownImGui();
    Vk_GfxPipelineCache::DestroyScenePipelines( *this );
    mySceneGpuCtx.mySceneDeletionQueue.flush();
    mySceneGpuCtx.myResourceTables.Clear();

    mySceneGpuCtx.myDescriptorPool        = VK_NULL_HANDLE;
    mySceneGpuCtx.myTextureSampler        = VK_NULL_HANDLE;
    mySceneGpuCtx.myBindlessDescriptorSet = VK_NULL_HANDLE;
    mySceneGpuCtx.myMaterialTableBuffer   = {};
    mySceneGpuCtx.myMaterialDescriptorSets.clear();
    mySceneGpuCtx.myMaterialParamBuffers.clear();
    for ( Vk_FrameData& frame : myFrameCtx.myFrameDatas ) {
        frame.myGlobalDescriptors.fill( VK_NULL_HANDLE );
        frame.myObjectDescriptor = VK_NULL_HANDLE;
    }

    mySceneGpuCtx.myDrawPrep.ClearFrameOutputs();
    mySceneGpuCtx.myDrawPrep.ResetLogState();
    Gfx_SetMaterialTableGenerationForExtract( 0 );

    World().ClearCpuSceneState();
    DebugUI().myScenePanel.myCurrentScenePath.clear();

    myMaterialBindLoggedOnce = false;
    myBindlessLoggedOnce     = false;
    myM1PerfLoggedOnce       = false;

    UtilLogger::Info( "SCENE", "UnloadScene: GPU scene resources released." );
}

void Vk_Core::InitImGui() {
    const uint32_t imageCount    = static_cast< uint32_t >( mySwapchainCtx.mySwapChainImageViews.size() );
    const uint32_t minImageCount = std::max( 2u, imageCount );

    myPlatformCtx.myImGuiLayer.Init( myPlatformCtx.myWindow, myDeviceCtx.myInstance, myDeviceCtx.myPhysicalDevice, myDeviceCtx.myDevice,
                                     myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), myDeviceCtx.myGraphicsQueue, mySwapchainCtx.mySwapChainImageFormat,
                                     mySwapchainCtx.mySwapChainExtent, mySwapchainCtx.mySwapChainImageViews, imageCount, minImageCount );
    UtilLogger::Info( "IMGUI", "ImGui overlay initialized." );
}

void Vk_Core::ShutdownImGui() {
    myPlatformCtx.myImGuiLayer.Shutdown();
    UtilLogger::Info( "IMGUI", "ImGui overlay shut down." );
}

void Vk_Core::CreateInstance() {
    UtilValidationLayers::LogInstanceLayerDiscovery();

    if ( myDeviceCtx.myEnableValidationLayers ) {
        UtilLogger::Info( "VULKAN", "Validation layers: enabled" );
        if ( !CheckValidationLayerSupport() ) {
            UtilLogger::Warn( "VULKAN", "Validation layers requested but unavailable. Continuing with validation disabled." );
            myDeviceCtx.myEnableValidationLayers = false;
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
    if ( ( myDeviceCtx.myEnableValidationLayers || myPlatformCtx.myRenderDoc.WantsDebugUtilsExtension() ) && UtilDebugMessenger::IsExtensionAvailable() ) {
        instanceExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
    }
    // Enables vkGetPhysicalDeviceFeatures2 during bindless probe (avoids loader emulation + bogus limits).
    Vk_AppendRequiredInstanceExtensions( instanceExtensions );

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( instanceExtensions.size() );
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if ( myDeviceCtx.myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myDeviceCtx.myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myDeviceCtx.myValidationLayers.data();
        UtilDebugMessenger::SetupForInstanceCreate( createInfo );
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

#ifdef _DEBUG
    CheckExtensionSupport();
#endif  // _DEBUG

    if ( vkCreateInstance( &createInfo, nullptr, &myDeviceCtx.myInstance ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateInstance failed." );
        throw std::runtime_error( "failed to create instance!" );
    }
    UtilLogger::Info( "VULKAN", "Vulkan instance created." );

    if ( myDeviceCtx.myEnableValidationLayers ) {
        UtilDebugMessenger::Create( myDeviceCtx.myInstance );
    }
}

void Vk_Core::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( myDeviceCtx.myInstance, &deviceCount, nullptr );

    if ( deviceCount == 0 )
        throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

    std::vector< VkPhysicalDevice > devices( deviceCount );
    vkEnumeratePhysicalDevices( myDeviceCtx.myInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices ) {
        if ( CheckDeviceSuitable( device ) ) {
            myDeviceCtx.myPhysicalDevice = device;
            // Keep startup stable across GPUs/drivers first; revisit dynamic MSAA selection later.
            mySwapchainCtx.myMSAASamples = VK_SAMPLE_COUNT_1_BIT;
            break;
        }
    }

    if ( myDeviceCtx.myPhysicalDevice == VK_NULL_HANDLE )
        throw std::runtime_error( "failed to find a suitable GPU!" );

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties( myDeviceCtx.myPhysicalDevice, &props );
    UtilLogger::Info( "GPU", std::string( "Selected physical device: " ) + props.deviceName );
}

void Vk_Core::CreateLogicalDevice() {
    // Build one queue create-info per unique family (graphics/present/transfer may collapse to fewer families).
    std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;
    std::set< uint32_t > uniqueQueueFamilies = { myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), myDeviceCtx.myQueueFamilyIndices.myPresentFamily.value(),
                                                 myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value() };

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
    if ( myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ) {
        indexingFeatures.runtimeDescriptorArray                    = VK_TRUE;
        indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        // Bindless material set uses VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT on the texture array.
        indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features = deviceFeatures;
    features2.pNext    = myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &indexingFeatures ) : nullptr;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &features2 ) : nullptr;

    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.queueCreateInfoCount    = static_cast< uint32_t >( queueCreateInfos.size() );
    createInfo.pEnabledFeatures        = myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? nullptr : &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( myDeviceCtx.myDeviceExtensions.size() );
    createInfo.ppEnabledExtensionNames = myDeviceCtx.myDeviceExtensions.data();

    if ( myDeviceCtx.myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myDeviceCtx.myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myDeviceCtx.myValidationLayers.data();
    }
    else
        createInfo.enabledLayerCount = 0;

    if ( vkCreateDevice( myDeviceCtx.myPhysicalDevice, &createInfo, nullptr, &myDeviceCtx.myDevice ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateDevice failed." );
        throw std::runtime_error( "failed to create logical device!" );
    }
    UtilLogger::Info( "VULKAN", "Logical device created." );

    // Resolve queue handles after logical-device creation.
    vkGetDeviceQueue( myDeviceCtx.myDevice, myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), 0, &myDeviceCtx.myGraphicsQueue );
    vkGetDeviceQueue( myDeviceCtx.myDevice, myDeviceCtx.myQueueFamilyIndices.myPresentFamily.value(), 0, &myDeviceCtx.myPresentQueue );
    vkGetDeviceQueue( myDeviceCtx.myDevice, myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value(), 0, &myDeviceCtx.myTransferQueue );
}

void Vk_Core::CreateSurface() {
    if ( glfwCreateWindowSurface( myDeviceCtx.myInstance, myPlatformCtx.myWindow, nullptr, &myDeviceCtx.mySurface ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "glfwCreateWindowSurface failed." );
        throw std::runtime_error( "failed to create window surface!" );
    }
    UtilLogger::Info( "VULKAN", "Window surface created." );
}

void Vk_Core::RecreateSurface() {
    if ( myDeviceCtx.mySurface != VK_NULL_HANDLE ) {
        vkDestroySurfaceKHR( myDeviceCtx.myInstance, myDeviceCtx.mySurface, nullptr );
        myDeviceCtx.mySurface = VK_NULL_HANDLE;
    }
    CreateSurface();
}

void Vk_Core::SyncResourceContext() {
    myResourceContext.Bind( myDeviceCtx.myDevice, myDeviceCtx.myAllocator, myDeviceCtx.myPhysicalDevice, myDeviceCtx.myGraphicsQueue, myDeviceCtx.myTransferQueue,
                            myDeviceCtx.myGraphicsCommandPool, myDeviceCtx.myTransferCommandPool, myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value_or( 0 ),
                            myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value_or( 0 ) );
}

void Vk_Core::InitAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = myDeviceCtx.myPhysicalDevice;
    allocatorInfo.device         = myDeviceCtx.myDevice;
    allocatorInfo.instance       = myDeviceCtx.myInstance;
    vmaCreateAllocator( &allocatorInfo, &myDeviceCtx.myAllocator );
    SyncResourceContext();

    // Lifetime bound to core deletion queue (survives swapchain recreate).
    myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() { vmaDestroyAllocator( myDeviceCtx.myAllocator ); } );
}

void Vk_Core::CreateFrameData() {
    myFrameCtx.myFrameDatas.resize( MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        // Per-frame command buffer.
        const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( myDeviceCtx.myGraphicsCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );

        if ( vkAllocateCommandBuffers( myDeviceCtx.myDevice, &allocInfo, &myFrameCtx.myFrameDatas[ i ].myCommandBuffer ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate command buffers!" );
        }

        // Per-frame camera UBO slab (one slot per render view).
        VkDeviceSize bufferSize = static_cast< VkDeviceSize >( kGfxMaxRenderViews ) * static_cast< VkDeviceSize >( PadUniformBufferSize( sizeof( GpuCameraData ) ) );

        CreateBuffer( bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myCameraBuffer, true );

        // Per-frame sync primitives (fence starts signaled for first frame).
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if ( vkCreateSemaphore( myDeviceCtx.myDevice, &semaphoreInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myPresentSemaphore ) != VK_SUCCESS
             || vkCreateSemaphore( myDeviceCtx.myDevice, &semaphoreInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myRenderSemaphore ) != VK_SUCCESS
             || vkCreateFence( myDeviceCtx.myDevice, &fenceInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myRenderFence ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create semaphores/fence!" );
        }

        // Core-owned lifetime cleanup.
        myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            vmaDestroyBuffer( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myCameraBuffer.myBuffer, myFrameCtx.myFrameDatas[ i ].myCameraBuffer.myAllocation );
            vkDestroySemaphore( myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myPresentSemaphore, nullptr );
            vkDestroySemaphore( myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myRenderSemaphore, nullptr );
            vkDestroyFence( myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myRenderFence, nullptr );
        } );
    }
}

void Vk_Core::CreateInstanceSlabs() {
    const VkDeviceSize slabSize = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxInstanceSlabEntries ) * static_cast< VkDeviceSize >( InstanceSlabStride() );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( slabSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myObjectBuffer, true );

        void* mapped = nullptr;
        vmaMapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation, &mapped );
        myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped = mapped;

        myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped != nullptr ) {
                vmaUnmapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped = nullptr;
            }
            vmaDestroyBuffer( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myBuffer, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation );
        } );
    }

    UtilLogger::Info( "RESOURCE", "Instance slab: entries=" + std::to_string( VkDescriptorPolicy::kMaxInstanceSlabEntries )
                                      + " stride=" + std::to_string( InstanceSlabStride() ) + " bytes/frame=" + std::to_string( slabSize ) );
}

// M2 prep §A: persistently mapped indirect + template SSBO rings (CPU fill in FillDrawTemplates; P3 GPU cull reuses layout).
void Vk_Core::CreateDrawTemplateBuffers() {
    static_assert( sizeof( Gfx_DrawIndirectCommand ) == sizeof( VkDrawIndexedIndirectCommand ), "Gfx_DrawIndirectCommand must match Vulkan" );

    const VkDeviceSize indirectBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxDrawTemplateEntries ) * sizeof( VkDrawIndexedIndirectCommand );
    const VkDeviceSize templateBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxDrawTemplateEntries ) * sizeof( Gfx_DrawTemplate );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( indirectBytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer, true );
        CreateBuffer( templateBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer, true );

        void* indirectMapped = nullptr;
        void* templateMapped = nullptr;
        vmaMapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation, &indirectMapped );
        vmaMapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation, &templateMapped );
        myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped = indirectMapped;
        myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped = templateMapped;

        myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped != nullptr ) {
                vmaUnmapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped = nullptr;
            }
            if ( myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped != nullptr ) {
                vmaUnmapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped = nullptr;
            }
            vmaDestroyBuffer( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation );
            vmaDestroyBuffer( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation );
        } );
    }

    UtilLogger::Info( "RESOURCE", "Draw-template buffers: entries=" + std::to_string( VkDescriptorPolicy::kMaxDrawTemplateEntries )
                                      + " indirectBytes/frame=" + std::to_string( indirectBytes ) + " templateBytes/frame=" + std::to_string( templateBytes ) );
}

// P3: per SoA slot entity-record SSBO (CPU fill in FillEntityRecords; compute cull reads same layout).
void Vk_Core::CreateEntityRecordBuffers() {
    const VkDeviceSize recordBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxEntitySlots ) * sizeof( Gfx_EntityGpuRecord );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( recordBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer, true );

        void* recordMapped = nullptr;
        vmaMapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation, &recordMapped );
        myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped = recordMapped;

        myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped != nullptr ) {
                vmaUnmapMemory( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped = nullptr;
            }
            vmaDestroyBuffer( myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation );
        } );
    }

    UtilLogger::Info( "RESOURCE", "Entity-record buffer: slots=" + std::to_string( VkDescriptorPolicy::kMaxEntitySlots ) + " bytes/frame=" + std::to_string( recordBytes ) );
}

void Vk_Core::CreateUniformBuffers() {
    // Device-scoped env UBO slab (CPU defaults written at scene load ??Vk_SceneHost::InitScenePresentation).
    // Each in-flight frame uses a static slice offset (not UNIFORM_BUFFER_DYNAMIC).
    const size_t envDataBufferSize = MAX_FRAMES_IN_FLIGHT * PadUniformBufferSize( sizeof( GpuEnvironmentData ) );
    CreateBuffer( envDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, myEnvDataBuffer, true );

    myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() { vmaDestroyBuffer( myDeviceCtx.myAllocator, myEnvDataBuffer.myBuffer, myEnvDataBuffer.myAllocation ); } );
}

void Vk_Core::CreateCommandPool() {
    // Graphics command pool: frame command buffers + graphics-side one-shot commands.
    VkCommandPoolCreateInfo poolInfo =
        VkInit::CommandPoolCreateInfo( myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    if ( vkCreateCommandPool( myDeviceCtx.myDevice, &poolInfo, nullptr, &myDeviceCtx.myGraphicsCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphic command pool!" );
    }

    // Transfer command pool: staging copy path when transfer queue differs.
    poolInfo = VkInit::CommandPoolCreateInfo( myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    if ( vkCreateCommandPool( myDeviceCtx.myDevice, &poolInfo, nullptr, &myDeviceCtx.myTransferCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create transfer command pool!" );
    }
}

namespace {

float ElapsedMs( std::chrono::high_resolution_clock::time_point aStart, std::chrono::high_resolution_clock::time_point aEnd ) {
    return std::chrono::duration< float, std::milli >( aEnd - aStart ).count();
}

}  // namespace

bool Vk_Core::PrepareFrameCpu( WorldState& aWorld, const std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews, uint32_t aViewCount, Vk_FrameCpuPrepResult& aOut ) {
    aOut = Vk_FrameCpuPrepResult{};

    myPlatformCtx.myRenderDoc.BeginFrameCaptureIfRequested();

    Vk_FrameData& frameData = myFrameCtx.myFrameDatas[ myFrameCtx.myCurrentFrame ];
    aOut.myFrameData        = &frameData;

    const auto fenceWaitStart = std::chrono::high_resolution_clock::now();
    vkWaitForFences( myDeviceCtx.myDevice, 1, &frameData.myRenderFence, VK_TRUE, UINT64_MAX );
    aOut.myGpuFenceWaitMs = ElapsedMs( fenceWaitStart, std::chrono::high_resolution_clock::now() );

    if ( !Vk_SwapchainHost::AcquireNextImage( *this, frameData, aOut.myImageIndex ) ) {
        return false;
    }

    myFrameStats.ResetPerFrameCounters();

    aOut.myActiveViewCount         = std::min( aViewCount, kGfxMaxRenderViews );
    const uint32_t activeViewCount = aOut.myActiveViewCount;

    uint32_t       totalOpaqueDraws   = 0;
    uint32_t       totalTransDraws    = 0;
    uint32_t       totalOpaqueRuns    = 0;
    uint32_t       totalTransRuns     = 0;
    const uint32_t slabPartitionCount = std::max( 1u, activeViewCount );
    const uint32_t perViewMaxEntries  = std::max( 1u, VkDescriptorPolicy::kMaxInstanceSlabEntries / slabPartitionCount );
    const uint32_t perViewEntitySlots = std::max( 1u, VkDescriptorPolicy::kMaxEntitySlots / slabPartitionCount );

    aOut.mySceneSlotCount = static_cast< uint32_t >( aWorld.mySceneSoA.GetSlotCount() );
    for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
        Gfx_GpuCullPushConstants& cullParams = aOut.myGpuCullViews[ viewIndex ];
        cullParams.viewProj                  = aViews[ viewIndex ].myCamera.myProj * aViews[ viewIndex ].myCamera.myView;
        cullParams.viewLayerMask             = aViews[ viewIndex ].myView.myLayerMask;
        cullParams.slotCount                 = aOut.mySceneSlotCount;
        cullParams.outputBaseSlot            = viewIndex * perViewEntitySlots;
        cullParams.pad                       = 0;
    }

    // Scene-wide; FillEntityRecords logs on failure (fail-closed before per-view slab/template fill).
    if ( !mySceneGpuCtx.myDrawPrep.FillEntityRecords( aWorld.mySceneSoA, mySceneGpuCtx.myResourceTables, myFrameCtx.myCurrentFrame, myFrameCtx.myFrameDatas ) ) {
        return false;
    }

    for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
        Gfx_LodState  secondaryViewLodState;
        Gfx_LodState* lodStateForView = &aWorld.myLodState;
        if ( viewIndex > 0 ) {
            secondaryViewLodState = aWorld.myLodState;
            lodStateForView       = &secondaryViewLodState;
        }

        Vk_FrameDrawPrepBuildParams prepParams{};
        prepParams.myScene                  = &aWorld.mySceneSoA;
        prepParams.myCamera                 = &aViews[ viewIndex ].myCamera;
        prepParams.myLodTable               = &aWorld.myLodTable;
        prepParams.myLodState               = lodStateForView;
        prepParams.myLodEnabled             = myDebugUI != nullptr ? myDebugUI->myRenderDebug.myLodEnabled : false;
        prepParams.myLodDebugLogicalMeshId  = aWorld.myLodDebugLogicalMeshId;
        prepParams.myCurrentFrame           = myFrameCtx.myCurrentFrame;
        prepParams.myFrameDatas             = &myFrameCtx.myFrameDatas;
        prepParams.myInstanceSlabStride     = InstanceSlabStride();
        prepParams.myInstanceSlabBaseOffset = static_cast< size_t >( viewIndex ) * static_cast< size_t >( perViewMaxEntries ) * InstanceSlabStride();
        prepParams.myInstanceSlabMaxEntries = perViewMaxEntries;
        prepParams.myDrawBufferBaseIndex    = viewIndex * perViewMaxEntries;
        prepParams.myDrawBufferMaxEntries   = perViewMaxEntries;
        prepParams.myViewLayerMask          = aViews[ viewIndex ].myView.myLayerMask;
        prepParams.myResourceTables         = &mySceneGpuCtx.myResourceTables;

        mySceneGpuCtx.myDrawPrep.ClearFrameOutputs();
        // Fail-closed on slab overflow (same post-acquire contract as swapchain acquire failure).
        if ( !mySceneGpuCtx.myDrawPrep.Build( prepParams ) ) {
            if ( !myPrepareFrameSlabOverflowLogged ) {
                UtilLogger::Warn( "RESOURCE", "PrepareFrameCpu aborted: instance slab overflow." );
                myPrepareFrameSlabOverflowLogged = true;
            }
            return false;
        }
        aOut.myViewPackets[ viewIndex ]      = mySceneGpuCtx.myDrawPrep.myFramePacket;
        aOut.myViewports[ viewIndex ]        = aViews[ viewIndex ].myViewport;
        aOut.myScissors[ viewIndex ]         = aViews[ viewIndex ].myScissor;
        aOut.myFrameDescriptors[ viewIndex ] = myFrameCtx.myFrameDatas[ myFrameCtx.myCurrentFrame ].myGlobalDescriptors[ viewIndex ];
        totalOpaqueDraws += static_cast< uint32_t >( mySceneGpuCtx.myDrawPrep.myFramePacket.myOpaquePass.myDraws.size() );
        totalTransDraws += static_cast< uint32_t >( mySceneGpuCtx.myDrawPrep.myFramePacket.myTransparentPass.myDraws.size() );
        totalOpaqueRuns += static_cast< uint32_t >( mySceneGpuCtx.myDrawPrep.myFramePacket.myOpaquePass.myBatchRuns.size() );
        totalTransRuns += static_cast< uint32_t >( mySceneGpuCtx.myDrawPrep.myFramePacket.myTransparentPass.myBatchRuns.size() );
        Vk_FrameUniformUploader::UpdateForView( *this, myFrameCtx.myCurrentFrame, viewIndex, aViews[ viewIndex ].myCamera );
    }

    aOut.myTotalOpaqueDraws      = totalOpaqueDraws;
    aOut.myTotalTransparentDraws = totalTransDraws;
    myFrameStats.SetDrawStreamMetrics( static_cast< uint32_t >( aWorld.mySceneSoA.GetActiveCount() ), totalOpaqueDraws, totalTransDraws, totalOpaqueRuns, totalTransRuns );

    const uint32_t visibleDrawsForPerf = totalOpaqueDraws + totalTransDraws;
    UtilPerfLog::AppendFrame( EngineConfig(), static_cast< uint64_t >( myFrameCtx.myFrameNumber ), myFrameStats.myFrameMs, myFrameStats.myDrawCalls, visibleDrawsForPerf,
                              aOut.myActiveViewCount, Vk_RenderMaterialPathName( myDeviceCtx.myMaterialPath ) );

    if ( !myMaterialBindLoggedOnce ) {
        if ( myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
            UtilLogger::Info( "DESCRIPTOR", "Set 1 bindless material set (once per pass); per-draw materialIndex in Set 2 slab." );
        }
        else {
            UtilLogger::Info( "DESCRIPTOR", "Set 1 material binds this frame will be <= batch runs (" + std::to_string( totalOpaqueRuns + totalTransRuns ) + ")" );
        }
        myMaterialBindLoggedOnce = true;
    }

    if ( !myBindlessLoggedOnce && myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        UtilLogger::Info( "BINDLESS", "recording with materialTableGeneration=" + std::to_string( mySceneGpuCtx.myResourceTables.GetMaterialTableGeneration() )
                                          + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds ) );
        myBindlessLoggedOnce = true;
    }

    aOut.myOk = true;
    return true;
}

Vk_FrameResult Vk_Core::DrawFrameGpu( const DebugUIState& aDebugUI, Vk_FrameCpuPrepResult& aPrep ) {
    ( void )aDebugUI;
    if ( !aPrep.myOk || aPrep.myFrameData == nullptr ) {
        return Vk_FrameResult::SkipFrame;
    }

    Vk_FrameData& frameData = *aPrep.myFrameData;

    // Env UBO after debug panels patch myEnvironmentData; camera slices already in PrepareFrameCpu.
    Vk_FrameUniformUploader::UpdateEnvironment( *this, myFrameCtx.myCurrentFrame );

    vkResetFences( myDeviceCtx.myDevice, 1, &frameData.myRenderFence );
    vkResetCommandBuffer( frameData.myCommandBuffer, 0 );

    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( 0 );
    if ( vkBeginCommandBuffer( frameData.myCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to begin recording command buffer!" );
    }

    Vk_GpuCull::RecordDispatches( *this, frameData.myCommandBuffer, aPrep );

    Vk_ScenePasses::RecordScene( *this, aDebugUI, frameData.myCommandBuffer, aPrep.myImageIndex, aPrep.myViewports, aPrep.myScissors, aPrep.myFrameDescriptors,
                                 aPrep.myActiveViewCount, aPrep.myViewPackets );

    // Lighting tuning applies next frame (unchanged contract).
    UtilLightingPanel::Build( myEnvironmentData );

    ImGui::Render();
    Vk_ScenePasses::RecordImGui( *this, frameData.myCommandBuffer, aPrep.myImageIndex );

    if ( vkEndCommandBuffer( frameData.myCommandBuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to record command buffer!" );
    }

    const Vk_FrameResult frameResult = Vk_SwapchainHost::SubmitAndPresent( *this, frameData, aPrep.myImageIndex );

    if ( myPlatformCtx.myHasFrameInputSampleTime ) {
        const float inputToPresentMs = ElapsedMs( myPlatformCtx.myFrameInputSampleTime, std::chrono::high_resolution_clock::now() );
        myFrameStats.RecordInputLatency( inputToPresentMs, aPrep.myGpuFenceWaitMs, myVsync, myFrameStats.myFrameMs );
        myPlatformCtx.myHasFrameInputSampleTime = false;
    }

    if ( !myM1PerfLoggedOnce && myFrameCtx.myFrameNumber >= 59 ) {
        LogM1PerfSnapshot();
        myM1PerfLoggedOnce = true;
    }

    myFrameCtx.myFrameNumber++;
    myFrameCtx.myCurrentFrame = myFrameCtx.myFrameNumber % MAX_FRAMES_IN_FLIGHT;
    return frameResult;
}

void Vk_Core::CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const {
    myPlatformCtx.myRenderDoc.CmdBeginDebugLabel( aCommandBuffer, aLabelName );
}

void Vk_Core::CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const {
    myPlatformCtx.myRenderDoc.CmdEndDebugLabel( aCommandBuffer );
}

bool Vk_Core::AreCommandDebugLabelsEnabled() const {
    return myPlatformCtx.myRenderDoc.AreCommandLabelsEnabled();
}

void Vk_Core::LogM1PerfSnapshot() const {
    const uint32_t visibleDraws = myFrameStats.myVisibleOpaqueDraws + myFrameStats.myVisibleTransparentDraws;
    const uint32_t batchRuns    = myFrameStats.myOpaqueBatchRuns + myFrameStats.myTransparentBatchRuns;
    UtilLogger::Info( "PERF", "frameMs=" + std::to_string( myFrameStats.myFrameMs ) + " fps=" + std::to_string( myFrameStats.myFps )
                                  + " entities=" + std::to_string( myFrameStats.myActiveEntities ) + " visibleDraws=" + std::to_string( visibleDraws )
                                  + " batchRuns=" + std::to_string( batchRuns ) + " (opaque=" + std::to_string( myFrameStats.myOpaqueBatchRuns ) + " transparent="
                                  + std::to_string( myFrameStats.myTransparentBatchRuns ) + ")" + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds )
                                  + " pipelineBinds=" + std::to_string( myFrameStats.myPipelineBinds ) + " drawCalls=" + std::to_string( myFrameStats.myDrawCalls )
                                  + " materialPath=" + Vk_RenderMaterialPathName( myDeviceCtx.myMaterialPath ) );
}

void Vk_Core::RefreshMaterialPipelinesAfterSwapchainRecreate() {
    const VkPipeline opaquePipe = myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBasicPipelineBindless : mySceneGpuCtx.myBasicPipeline;
    const VkPipeline transPipe =
        myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myTransparentPipelineBindless : mySceneGpuCtx.myTransparentPipeline;
    const VkPipelineLayout layout = myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBindlessPipelineLayout : mySceneGpuCtx.myPipelineLayout;
    mySceneGpuCtx.myResourceTables.RefreshMaterialPipelines( opaquePipe, transPipe, layout );
}

void Vk_Core::InitVk_QueueFamilyIndices() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( myDeviceCtx.myPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( myDeviceCtx.myPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( myDeviceCtx.myPhysicalDevice, i, myDeviceCtx.mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myDeviceCtx.myQueueFamilyIndices.myPresentFamily  = i;
            myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily = i;
        }

        // Prefer a dedicated transfer-only family when available (staging uploads).
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myDeviceCtx.myQueueFamilyIndices.myTransferFamily = i;
        }

        if ( myDeviceCtx.myQueueFamilyIndices.isComplete() )
            break;

        i++;
    }

    myDeviceCtx.myQueueFamilyIndices.ApplyTransferFallback();

    const uint32_t graphicsFamily = myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value_or( 0 );
    const uint32_t transferFamily = myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value_or( graphicsFamily );
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
    return UtilValidationLayers::AreLayersAvailable( myDeviceCtx.myValidationLayers );
}

void Vk_Core::SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers ) {
    myDeviceCtx.myEnableValidationLayers = aEnableValidationLayers;
    myDeviceCtx.myValidationLayers       = someValidationLayers;
}

void Vk_Core::SetRequiredExtension( std::vector< const char* > someDeviceExtensions ) {
    myDeviceCtx.myDeviceExtensions = someDeviceExtensions;
}

bool Vk_Core::CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const {
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &myDeviceCtx.myPhysicalDeviceProperties );

    vkGetPhysicalDeviceFeatures( aPhysicalDevice, &myDeviceCtx.myPhysicalDeviceFeatures );

    Vk_QueueFamilyIndices indices = FindQueueFamilies( aPhysicalDevice );

    bool extensionSupported = CheckExtensionSupport( aPhysicalDevice );

    // Swapchain formats + present modes are required for renderable output.
    bool swapChainAdequate = false;
    if ( extensionSupported ) {
        Vk_SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( aPhysicalDevice );
        swapChainAdequate                           = !swapChainSupport.myFormats.empty() && !swapChainSupport.myPresentModes.empty();
    }

#ifdef _DEBUG
    UtilLogger::Debug( "GPU", "minUniformBufferOffsetAlignment=" + std::to_string( myDeviceCtx.myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment ) );
#endif  // _DEBUG

    return indices.isComplete() && extensionSupported && swapChainAdequate && myDeviceCtx.myPhysicalDeviceFeatures.samplerAnisotropy;
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
        vkGetPhysicalDeviceSurfaceSupportKHR( aPhysicalDevice, i, myDeviceCtx.mySurface, &presentSupport );
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( aPhysicalDevice, myDeviceCtx.mySurface, &details.myCapabilities );

    // Supported color formats/colorspaces for present images.
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, myDeviceCtx.mySurface, &formatCount, nullptr );

    if ( formatCount != 0 ) {
        details.myFormats.resize( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, myDeviceCtx.mySurface, &formatCount, details.myFormats.data() );
    }

    // Supported present modes (FIFO/MAILBOX/IMMEDIATE).
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, myDeviceCtx.mySurface, &presentModeCount, nullptr );

    if ( presentModeCount != 0 ) {
        details.myPresentModes.resize( presentModeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, myDeviceCtx.mySurface, &presentModeCount, details.myPresentModes.data() );
    }

    return details;
}

bool Vk_Core::CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > availableExtensions( extensionCount );
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, availableExtensions.data() );

    std::set< std::string > requiredExtensions( myDeviceCtx.myDeviceExtensions.begin(), myDeviceCtx.myDeviceExtensions.end() );

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
        glfwGetFramebufferSize( myPlatformCtx.myWindow, &width, &height );

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
    UtilVulkanResult::ThrowOnFailure( vkCreateShaderModule( myDeviceCtx.myDevice, &createInfo, nullptr, &shaderModule ), "vkCreateShaderModule" );

    return shaderModule;
}

VkShaderModule Vk_Core::CreateShaderModule( const std::string aShaderPath ) const {
    UtilLogger::Info( "SHADER", "Loading shader module: " + aShaderPath );
    const auto shaderCode = UtilLoader::ReadFile( EngineConfig(), aShaderPath );

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( shaderCode.data() );

    VkShaderModule shaderModule;
    const VkResult moduleResult = vkCreateShaderModule( myDeviceCtx.myDevice, &createInfo, nullptr, &shaderModule );
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

    vkCore->mySwapchainCtx.myFramebufferResized = true;
}

uint32_t Vk_Core::FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( myDeviceCtx.myPhysicalDevice, &memProperties );

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( aTypeFiler & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & someProperties ) == someProperties ) {
            return i;
        }
    }

    throw std::runtime_error( "failed to find suitable memory type!" );
}

void Vk_Core::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool isExclusive ) const {
    myResourceContext.CreateBuffer( aSize, aBufferUsage, aMemoryUsage, aBuffer, isExclusive );
}

void Vk_Core::CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myResourceContext.CopyBuffer( aSrcBuffer, aDstBuffer, aSize );
}

void Vk_Core::CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myResourceContext.CopyBufferOnGraphicsQueue( aSrcBuffer, aDstBuffer, aSize );
}

size_t Vk_Core::InstanceSlabStride() const {
    return PadUniformBufferSize( sizeof( GpuObjectData ) );
}

// Per-frame UBO upload is delegated to Vk_FrameUniformUploader.
void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                           Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, 1, VK_SAMPLE_COUNT_1_BIT, anImage );
}

void Vk_Core::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Core::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                           VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myResourceContext.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Core::TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    myResourceContext.TransitionImageLayout( aImage, aFormat, anOldLayout, aNewLayout, aMipLevel );
}

void Vk_Core::CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const {
    myResourceContext.CopyBufferToImage( aBuffer, aImage, aWidth, aHeight );
}

void Vk_Core::GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const {
    myResourceContext.GenerateMipmaps( aImage, aImageFormat, aTexWidth, aTexHeight, aMipLevel );
}

VkImageView Vk_Core::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    return myResourceContext.CreateImageView( anImage, aFormat, anAspect, aMipLevel );
}

VkFormat Vk_Core::FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const {
    for ( const VkFormat format : someCandidates ) {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties( myDeviceCtx.myPhysicalDevice, format, &properties );
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

VkSampleCountFlagBits Vk_Core::GetMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties( myDeviceCtx.myPhysicalDevice, &physicalDeviceProperties );

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
    myPlatformCtx.myFrameInputSampleTime    = aSampleTime;
    myPlatformCtx.myHasFrameInputSampleTime = true;
}

size_t Vk_Core::PadUniformBufferSize( size_t anOriginalSize ) const {
    // Slab stride / future UNIFORM_BUFFER_DYNAMIC offsets - multiples of minUniformBufferOffsetAlignment.
    size_t minUboAlignment = myDeviceCtx.myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize     = anOriginalSize;

    if ( minUboAlignment > 0 )
        alignedSize = ( alignedSize + minUboAlignment - 1 ) & ~( minUboAlignment - 1 );

    return alignedSize;
}

#pragma endregion

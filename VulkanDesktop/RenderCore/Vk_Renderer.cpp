// Module: Vk_Renderer - device, swapchain, frame loop (acquire -> prep -> record -> present).
// Scene CPU: App (WorldState). View packets: Gfx_BuildViewFramePacket. GPU upload prep: Vk_FrameDrawPrep.
#include "Vk_Renderer.h"
#include "../App/App_PlatformHost.h"
#include "../Gfx/Gfx_Bounds.h"
#include "../Gfx/Gfx_DrawTemplate.h"
#include "../Gfx/Gfx_EntityGpuRecord.h"
#include "../Gfx/Gfx_GpuCull.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "../Gfx/Gfx_SceneApply.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Util/Util_DebugMessenger.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_PerfLog.h"
#include "../Util/Util_ValidationLayers.h"
#include "../Util/Util_VulkanResult.h"
#include "Vk_Bindless.h"
#include "Vk_DescriptorSystem.h"
#include "Vk_DevicePipelineCache.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_GBufferPass.h"
#include "Vk_GfxPipelineCache.h"
#include "Vk_GpuCull.h"
#include "Vk_IblResources.h"
#include "Vk_PipelineDiagnostics.h"
#include "Vk_RenderDevice.h"
#include "Vk_ScenePasses.h"
#include "Vk_ShadowMapPass.h"
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

Vk_Renderer::Vk_Renderer() {}
Vk_Renderer::~Vk_Renderer() {}

// Called once from Application before scene load / main loop (non-owning; must outlive Vk_Renderer).
void Vk_Renderer::BindEngineConfig( const Util_EngineConfig* aConfig ) {
    myEngineConfig = aConfig;
}

const Util_EngineConfig& Vk_Renderer::EngineConfig() const {
    if ( myEngineConfig == nullptr ) {
        throw std::runtime_error( "Vk_Renderer: Util_EngineConfig not bound (call BindEngineConfig from Application)" );
    }
    return *myEngineConfig;
}

Gfx_Bounds Vk_Renderer::GetShadowCasterBounds() const {
    if ( myBoundSceneSoA == nullptr ) {
        return {};
    }
    return Gfx_ComputeActiveOpaqueSceneBounds( *myBoundSceneSoA );
}

const std::string& Vk_Renderer::GetLoadedSceneLogicalPath() const {
    return myLoadedSceneLogicalPath;
}

bool Vk_Renderer::HasLoadedScene() const {
    return mySceneGpuLoaded;
}

void Vk_Renderer::SetSize( const uint32_t aWidth, const uint32_t aHeight ) {
    myPlatformCtx.myWidth  = aWidth;
    myPlatformCtx.myHeight = aHeight;
}

void Vk_Renderer::SetVsync( bool aVsync ) {
    myVsync = aVsync;
}

void Vk_Renderer::Reset() {
    Clear();
}

void Vk_Renderer::ConfigureRenderDoc( bool aEnableRenderDoc ) {
    myPlatformCtx.myRenderDoc.Configure( aEnableRenderDoc );
}

bool Vk_Renderer::IsRenderDocEnabled() const {
    return myPlatformCtx.myRenderDoc.IsEnabled();
}

void Vk_Renderer::TriggerRenderDocCapture() {
    myPlatformCtx.myRenderDoc.TriggerCaptureRequest();
}

void Vk_Renderer::Shutdown() {
    if ( myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myRhi.myDeviceCtx.myDevice );
    }
    Clear();
}

void Vk_Renderer::Clear() {
    UtilLogger::Info( "CORE", "Releasing Vulkan resources." );
    // Destruction order matters: child GPU objects -> device -> surface/debug -> instance -> window.

    ShutdownImGui();

    mySwapchainCtx.mySwapChainDeletionQueue.flush();
    mySceneGpuCtx.mySceneDeletionQueue.flush();
    myRhi.myDeviceCtx.myDeletionQueue.flush();
    mySceneGpuCtx.myResourceTables.Clear();

    vkDestroyCommandPool( myRhi.myDeviceCtx.myDevice, myRhi.myDeviceCtx.myGraphicsCommandPool, nullptr );
    vkDestroyCommandPool( myRhi.myDeviceCtx.myDevice, myRhi.myDeviceCtx.myTransferCommandPool, nullptr );

    // Persist pipeline cache blob while device is still valid.
    Vk_DevicePipelineCache::Destroy( *this );

    vkDestroyDevice( myRhi.myDeviceCtx.myDevice, nullptr );

    vkDestroySurfaceKHR( myRhi.myDeviceCtx.myInstance, myRhi.myDeviceCtx.mySurface, nullptr );

    UtilDebugMessenger::Destroy( myRhi.myDeviceCtx.myInstance );
    vkDestroyInstance( myRhi.myDeviceCtx.myInstance, nullptr );

    myPlatformCtx.myWindow = nullptr;
    myPlatformCtx.myRenderDoc.Shutdown();
    UtilLogger::Info( "CORE", "Resource cleanup completed." );
}

// Render device bootstrap entry point (instance/device/queues/swapchain host orchestration).
void Vk_Renderer::InitRenderDevice() {
    UtilLogger::Info( "VULKAN", "InitRenderDevice: instance, device, swapchain (no scene resources)." );
    // RenderDoc in-app API should be discovered before Vulkan instance/device initialization.
    myPlatformCtx.myRenderDoc.InitRuntime();
    if ( myPlatformHost == nullptr ) {
        throw std::runtime_error( "Vk_Renderer::InitRenderDevice requires bound App_PlatformHost" );
    }
    Vk_RenderDevice::Init( *this, *myPlatformHost );
    myPlatformCtx.myRenderDoc.BindVulkanHandles( myRhi.myDeviceCtx.myDevice );

    Vk_SwapchainHost::Init( *this );

    CreateFrameData();
    CreateInstanceSlabs();
    CreateDrawTemplateBuffers();
    CreateEntityRecordBuffers();
    Vk_GpuCull::Init( *this );
    Vk_GpuCull::CreateFrameBuffers( *this );
    CreateUniformBuffers();
    SyncResourceContext();
    Vk_DescriptorSystem::InitDeviceLayouts( *this );
    myLightingSettings = EngineConfig().GetLightingSettings();
    Vk_IblResources::Init( *this, EngineConfig().GetEnvironmentPath() );
    Vk_ShadowMapPass::Init( *this );
    UtilLogger::Info( "VULKAN", "InitRenderDevice completed." );
}

// Scene-load orchestration: GPU pipelines + resource tables + descriptors (CPU scene state is App-owned).
void Vk_Renderer::LoadSceneGpuResources( const Gfx_SceneGpuLoadParams& aLoadParams ) {
    if ( mySceneGpuLoaded ) {
        throw std::runtime_error( "LoadSceneGpuResources: scene already loaded; call UnloadSceneGpuResources first." );
    }
    if ( aLoadParams.mySceneDesc == nullptr || aLoadParams.mySceneIdTables == nullptr || aLoadParams.mySceneSoA == nullptr ) {
        throw std::runtime_error( "LoadSceneGpuResources: invalid Gfx_SceneGpuLoadParams (null pointers)." );
    }

    UtilLogger::Info( "SCENE", "LoadSceneGpuResources." );
    myLoadedSceneLogicalPath = aLoadParams.myLogicalPath;
    myBoundSceneSoA          = aLoadParams.mySceneSoA;

    ( void )Gfx_GetSceneShader( *aLoadParams.mySceneDesc, "lit" );
    const Gfx_ShaderPermutationDef& activePerm = Gfx_ShaderPermutation::GetActiveDefinition();
    mySceneVertShaderPath                      = activePerm.myVertSpvLogicalPath;
    mySceneFragShaderPath                      = activePerm.myFragSpvLogicalPath;

    Vk_GfxPipelineCache::InitScenePipelines( *this );

    if ( Gfx_RenderPreset::IsHybridDeferred( EngineConfig().GetRenderPresetName() ) ) {
        Vk_GBufferPass::Init( *this );
        Vk_ClusterBuildPass::Init( *this );
        Vk_DepthPyramidPass::Init( *this );
        Vk_SsrPass::Init( *this );
        Vk_AoPass::Init( *this );
        Vk_ShadowAoSoftPass::Init( *this );
        Vk_PostProcessPass::Init( *this );
        Vk_GfxPipelineCache::CreateHybridResolveGfxPipelines( *this );
        Vk_DeferredLightingPass::Init( *this );
    }

    {
        Gfx_ResourceManifest manifest{};
        Gfx_BuildResourceManifestFromSceneDesc( *aLoadParams.mySceneDesc, *aLoadParams.mySceneIdTables, manifest );
        mySceneGpuCtx.myTextureImageMipLevels = 1;
        const VkPipeline opaquePipe =
            myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBasicPipelineBindless : mySceneGpuCtx.myBasicPipeline;
        const VkPipeline transPipe =
            myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myTransparentPipelineBindless : mySceneGpuCtx.myTransparentPipeline;
        const VkPipelineLayout layout =
            myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBindlessPipelineLayout : mySceneGpuCtx.myPipelineLayout;
        SyncResourceContext();
        mySceneGpuCtx.myResourceTables.LoadFromManifest( EngineConfig(), manifest, myRhi.myResourceContext, mySceneGpuCtx.mySceneDeletionQueue,
                                                         mySceneGpuCtx.myTextureImageMipLevels, opaquePipe, transPipe, layout );
        Gfx_ApplyMeshLocalBoundsToSceneSoA( *aLoadParams.mySceneDesc, *aLoadParams.mySceneIdTables, mySceneGpuCtx.myResourceTables.CollectMeshLocalBounds(),
                                            *aLoadParams.mySceneSoA );
    }

    Vk_DescriptorSystem::InitSceneDescriptors( *this );
    Gfx_SetMaterialTableGenerationForExtract( mySceneGpuCtx.myResourceTables.GetMaterialTableGeneration() );
    InitImGui();
    mySceneGpuLoaded = true;
    UtilLogger::Info( "SCENE", "LoadSceneGpuResources completed." );
}

void Vk_Renderer::UnloadSceneGpuResources() {
    if ( !mySceneGpuLoaded ) {
        UtilLogger::Debug( "SCENE", "UnloadSceneGpuResources: no scene loaded (skipped)." );
        return;
    }

    UtilLogger::Info( "SCENE", "UnloadSceneGpuResources: releasing GPU scene resources." );
    if ( myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( myRhi.myDeviceCtx.myDevice );
    }

    ShutdownImGui();
    Vk_DeferredLightingPass::Destroy( *this );
    Vk_PostProcessPass::Destroy( *this );
    Vk_ShadowAoSoftPass::Destroy( *this );
    Vk_AoPass::Destroy( *this );
    Vk_SsrPass::Destroy( *this );
    Vk_DepthPyramidPass::Destroy( *this );
    Vk_ClusterBuildPass::Destroy( *this );
    Vk_GBufferPass::Destroy( *this );
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

    myLoadedSceneLogicalPath.clear();
    myBoundSceneSoA  = nullptr;
    mySceneGpuLoaded = false;

    myMaterialBindLoggedOnce = false;
    myBindlessLoggedOnce     = false;
    myM1PerfLoggedOnce       = false;

    UtilLogger::Info( "SCENE", "UnloadSceneGpuResources: GPU scene resources released." );
}

void Vk_Renderer::InitImGui() {
    const uint32_t imageCount    = static_cast< uint32_t >( mySwapchainCtx.mySwapChainImageViews.size() );
    const uint32_t minImageCount = std::max( 2u, imageCount );

    myPlatformCtx.myImGuiLayer.Init( myPlatformCtx.myWindow, myRhi.myDeviceCtx.myInstance, myRhi.myDeviceCtx.myPhysicalDevice, myRhi.myDeviceCtx.myDevice,
                                     myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), myRhi.myDeviceCtx.myGraphicsQueue, mySwapchainCtx.mySwapChainImageFormat,
                                     mySwapchainCtx.mySwapChainExtent, mySwapchainCtx.mySwapChainImageViews, imageCount, minImageCount );
    UtilLogger::Debug( "IMGUI", "ImGui overlay initialized." );
}

void Vk_Renderer::ShutdownImGui() {
    myPlatformCtx.myImGuiLayer.Shutdown();
    UtilLogger::Debug( "IMGUI", "ImGui overlay shut down." );
}

void Vk_Renderer::CreateInstance() {
    UtilValidationLayers::LogInstanceLayerDiscovery();

    if ( myRhi.myDeviceCtx.myEnableValidationLayers ) {
        UtilLogger::Info( "VULKAN", "Validation layers: enabled" );
        if ( !CheckValidationLayerSupport() ) {
            UtilLogger::Warn( "VULKAN", "Validation layers requested but unavailable. Continuing with validation disabled." );
            myRhi.myDeviceCtx.myEnableValidationLayers = false;
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
    if ( ( myRhi.myDeviceCtx.myEnableValidationLayers || myPlatformCtx.myRenderDoc.WantsDebugUtilsExtension() ) && UtilDebugMessenger::IsExtensionAvailable() ) {
        instanceExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
    }
    // Enables vkGetPhysicalDeviceFeatures2 during bindless probe (avoids loader emulation + bogus limits).
    Vk_AppendRequiredInstanceExtensions( instanceExtensions );

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( instanceExtensions.size() );
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    if ( myRhi.myDeviceCtx.myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myRhi.myDeviceCtx.myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myRhi.myDeviceCtx.myValidationLayers.data();
        UtilDebugMessenger::SetupForInstanceCreate( createInfo );
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

#ifdef _DEBUG
    CheckExtensionSupport();
#endif  // _DEBUG

    if ( vkCreateInstance( &createInfo, nullptr, &myRhi.myDeviceCtx.myInstance ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateInstance failed." );
        throw std::runtime_error( "failed to create instance!" );
    }
    UtilLogger::Info( "VULKAN", "Vulkan instance created." );

    if ( myRhi.myDeviceCtx.myEnableValidationLayers ) {
        UtilDebugMessenger::Create( myRhi.myDeviceCtx.myInstance );
    }
}

void Vk_Renderer::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices( myRhi.myDeviceCtx.myInstance, &deviceCount, nullptr );

    if ( deviceCount == 0 )
        throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

    std::vector< VkPhysicalDevice > devices( deviceCount );
    vkEnumeratePhysicalDevices( myRhi.myDeviceCtx.myInstance, &deviceCount, devices.data() );

    for ( const auto& device : devices ) {
        if ( CheckDeviceSuitable( device ) ) {
            myRhi.myDeviceCtx.myPhysicalDevice = device;
            // Keep startup stable across GPUs/drivers first; revisit dynamic MSAA selection later.
            mySwapchainCtx.myMSAASamples = VK_SAMPLE_COUNT_1_BIT;
            break;
        }
    }

    if ( myRhi.myDeviceCtx.myPhysicalDevice == VK_NULL_HANDLE )
        throw std::runtime_error( "failed to find a suitable GPU!" );

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties( myRhi.myDeviceCtx.myPhysicalDevice, &props );
    UtilLogger::Info( "GPU", std::string( "Selected physical device: " ) + props.deviceName );
}

void Vk_Renderer::CreateLogicalDevice() {
    // Build one queue create-info per unique family (graphics/present/transfer may collapse to fewer families).
    std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;
    std::set< uint32_t >                   uniqueQueueFamilies = { myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(),
                                                                   myRhi.myDeviceCtx.myQueueFamilyIndices.myPresentFamily.value(),
                                                                   myRhi.myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value() };

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
    if ( myRhi.myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ) {
        indexingFeatures.runtimeDescriptorArray                    = VK_TRUE;
        indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        // Bindless material set uses VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT on the texture array.
        indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features = deviceFeatures;
    features2.pNext    = myRhi.myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &indexingFeatures ) : nullptr;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = myRhi.myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? static_cast< void* >( &features2 ) : nullptr;

    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.queueCreateInfoCount    = static_cast< uint32_t >( queueCreateInfos.size() );
    createInfo.pEnabledFeatures        = myRhi.myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ? nullptr : &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast< uint32_t >( myRhi.myDeviceCtx.myDeviceExtensions.size() );
    createInfo.ppEnabledExtensionNames = myRhi.myDeviceCtx.myDeviceExtensions.data();

    if ( myRhi.myDeviceCtx.myEnableValidationLayers ) {
        createInfo.enabledLayerCount   = static_cast< uint32_t >( myRhi.myDeviceCtx.myValidationLayers.size() );
        createInfo.ppEnabledLayerNames = myRhi.myDeviceCtx.myValidationLayers.data();
    }
    else
        createInfo.enabledLayerCount = 0;

    if ( vkCreateDevice( myRhi.myDeviceCtx.myPhysicalDevice, &createInfo, nullptr, &myRhi.myDeviceCtx.myDevice ) != VK_SUCCESS ) {
        UtilLogger::Error( "VULKAN", "vkCreateDevice failed." );
        throw std::runtime_error( "failed to create logical device!" );
    }
    UtilLogger::Info( "VULKAN", "Logical device created." );

    // Resolve queue handles after logical-device creation.
    vkGetDeviceQueue( myRhi.myDeviceCtx.myDevice, myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), 0, &myRhi.myDeviceCtx.myGraphicsQueue );
    vkGetDeviceQueue( myRhi.myDeviceCtx.myDevice, myRhi.myDeviceCtx.myQueueFamilyIndices.myPresentFamily.value(), 0, &myRhi.myDeviceCtx.myPresentQueue );
    vkGetDeviceQueue( myRhi.myDeviceCtx.myDevice, myRhi.myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value(), 0, &myRhi.myDeviceCtx.myTransferQueue );
}

void Vk_Renderer::CreateSurface() {
    if ( myPlatformHost == nullptr ) {
        throw std::runtime_error( "Vk_Renderer::CreateSurface requires bound App_PlatformHost" );
    }
    myPlatformHost->CreateSurface( myRhi );
}

void Vk_Renderer::RecreateSurface() {
    if ( myPlatformHost == nullptr ) {
        throw std::runtime_error( "Vk_Renderer::RecreateSurface requires bound App_PlatformHost" );
    }
    myPlatformHost->RecreateSurface( myRhi );
}

void Vk_Renderer::SyncResourceContext() {
    myRhi.SyncResourceContext();
}

void Vk_Renderer::InitAllocator() {
    myRhi.InitAllocator();
}

void Vk_Renderer::CreateFrameData() {
    myFrameCtx.myFrameDatas.resize( MAX_FRAMES_IN_FLIGHT );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        // Per-frame command buffer.
        const VkCommandBufferAllocateInfo allocInfo = VkInit::CommandBufferAllocInfo( myRhi.myDeviceCtx.myGraphicsCommandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY );

        if ( vkAllocateCommandBuffers( myRhi.myDeviceCtx.myDevice, &allocInfo, &myFrameCtx.myFrameDatas[ i ].myCommandBuffer ) != VK_SUCCESS ) {
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

        if ( vkCreateSemaphore( myRhi.myDeviceCtx.myDevice, &semaphoreInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myPresentSemaphore ) != VK_SUCCESS
             || vkCreateSemaphore( myRhi.myDeviceCtx.myDevice, &semaphoreInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myRenderSemaphore ) != VK_SUCCESS
             || vkCreateFence( myRhi.myDeviceCtx.myDevice, &fenceInfo, nullptr, &myFrameCtx.myFrameDatas[ i ].myRenderFence ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to create semaphores/fence!" );
        }

        // Core-owned lifetime cleanup.
        myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myCameraBuffer.myBuffer, myFrameCtx.myFrameDatas[ i ].myCameraBuffer.myAllocation );
            vkDestroySemaphore( myRhi.myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myPresentSemaphore, nullptr );
            vkDestroySemaphore( myRhi.myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myRenderSemaphore, nullptr );
            vkDestroyFence( myRhi.myDeviceCtx.myDevice, myFrameCtx.myFrameDatas[ i ].myRenderFence, nullptr );
        } );
    }
}

void Vk_Renderer::CreateInstanceSlabs() {
    const VkDeviceSize slabSize = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxInstanceSlabEntries ) * static_cast< VkDeviceSize >( InstanceSlabStride() );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( slabSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myObjectBuffer, true );

        void* mapped = nullptr;
        vmaMapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation, &mapped );
        myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped = mapped;

        myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped != nullptr ) {
                vmaUnmapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myInstanceSlabMapped = nullptr;
            }
            vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myBuffer, myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myAllocation );
        } );
    }

    UtilLogger::Debug( "RESOURCE", "Instance slab: entries=" + std::to_string( VkDescriptorPolicy::kMaxInstanceSlabEntries )
                                       + " stride=" + std::to_string( InstanceSlabStride() ) + " bytes/frame=" + std::to_string( slabSize ) );
}

// M2 prep: persistently mapped indirect + template SSBO rings (CPU fill in FillDrawTemplates; P3 GPU cull reuses layout).
void Vk_Renderer::CreateDrawTemplateBuffers() {
    static_assert( sizeof( Gfx_DrawIndirectCommand ) == sizeof( VkDrawIndexedIndirectCommand ), "Gfx_DrawIndirectCommand must match Vulkan" );

    const VkDeviceSize indirectBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxDrawTemplateEntries ) * sizeof( VkDrawIndexedIndirectCommand );
    const VkDeviceSize templateBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxDrawTemplateEntries ) * sizeof( Gfx_DrawTemplate );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( indirectBytes, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer, true );
        CreateBuffer( templateBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer, true );

        void* indirectMapped = nullptr;
        void* templateMapped = nullptr;
        vmaMapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation, &indirectMapped );
        vmaMapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation, &templateMapped );
        myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped = indirectMapped;
        myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped = templateMapped;

        myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped != nullptr ) {
                vmaUnmapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myDrawIndirectMapped = nullptr;
            }
            if ( myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped != nullptr ) {
                vmaUnmapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myDrawTemplateMapped = nullptr;
            }
            vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myDrawIndirectBuffer.myAllocation );
            vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myDrawTemplateBuffer.myAllocation );
        } );
    }

    UtilLogger::Debug( "RESOURCE", "Draw-template buffers: entries=" + std::to_string( VkDescriptorPolicy::kMaxDrawTemplateEntries )
                                       + " indirectBytes/frame=" + std::to_string( indirectBytes ) + " templateBytes/frame=" + std::to_string( templateBytes ) );
}

// P3: per SoA slot entity-record SSBO (CPU fill in FillEntityRecords; compute cull reads same layout).
void Vk_Renderer::CreateEntityRecordBuffers() {
    const VkDeviceSize recordBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxEntitySlots ) * sizeof( Gfx_EntityGpuRecord );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        CreateBuffer( recordBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer, true );

        void* recordMapped = nullptr;
        vmaMapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation, &recordMapped );
        myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped = recordMapped;

        myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() {
            if ( myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped != nullptr ) {
                vmaUnmapMemory( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation );
                myFrameCtx.myFrameDatas[ i ].myEntityRecordMapped = nullptr;
            }
            vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myBuffer,
                              myFrameCtx.myFrameDatas[ i ].myEntityRecordBuffer.myAllocation );
        } );
    }

    UtilLogger::Debug( "RESOURCE", "Entity-record buffer: slots=" + std::to_string( VkDescriptorPolicy::kMaxEntitySlots ) + " bytes/frame=" + std::to_string( recordBytes ) );
}

void Vk_Renderer::CreateUniformBuffers() {
    // Device-scoped env UBO slab (CPU defaults at scene load; App_InitScenePresentation).
    // Each in-flight frame uses a static slice offset (not UNIFORM_BUFFER_DYNAMIC).
    const size_t envDataBufferSize = MAX_FRAMES_IN_FLIGHT * PadUniformBufferSize( sizeof( GpuEnvironmentData ) );
    CreateBuffer( envDataBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, myEnvDataBuffer, true );

    myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ = ]() { vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myEnvDataBuffer.myBuffer, myEnvDataBuffer.myAllocation ); } );

    const size_t lightingGlobalsSize = MAX_FRAMES_IN_FLIGHT * PadUniformBufferSize( sizeof( GpuLightingGlobals ) );
    CreateBuffer( lightingGlobalsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, myLightingGlobalsBuffer, true );
    myRhi.myDeviceCtx.myDeletionQueue.pushFunction(
        [ = ]() { vmaDestroyBuffer( myRhi.myDeviceCtx.myAllocator, myLightingGlobalsBuffer.myBuffer, myLightingGlobalsBuffer.myAllocation ); } );
}

void Vk_Renderer::CreateCommandPool() {
    // Graphics command pool: frame command buffers + graphics-side one-shot commands.
    VkCommandPoolCreateInfo poolInfo =
        VkInit::CommandPoolCreateInfo( myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );

    if ( vkCreateCommandPool( myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &myRhi.myDeviceCtx.myGraphicsCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create graphic command pool!" );
    }

    // Transfer command pool: staging copy path when transfer queue differs.
    poolInfo = VkInit::CommandPoolCreateInfo( myRhi.myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT );
    if ( vkCreateCommandPool( myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &myRhi.myDeviceCtx.myTransferCommandPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create transfer command pool!" );
    }
}

namespace {

float ElapsedMs( std::chrono::high_resolution_clock::time_point aStart, std::chrono::high_resolution_clock::time_point aEnd ) {
    return std::chrono::duration< float, std::milli >( aEnd - aStart ).count();
}

}  // namespace

bool Vk_Renderer::PrepareFrameCpu( const Gfx_FramePrepInput& aInput, const Gfx_FrameDebugToggles& aToggles,
                                   const std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews, uint32_t aViewCount,
                                   const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets, Vk_FrameCpuPrepResult& aOut ) {
    if ( aInput.myScene == nullptr || aInput.myLodTable == nullptr || aInput.myLodState == nullptr ) {
        throw std::runtime_error( "PrepareFrameCpu: invalid Gfx_FramePrepInput (null scene/lod pointers)" );
    }
    aOut = Vk_FrameCpuPrepResult{};

    myPlatformCtx.myRenderDoc.BeginFrameCaptureIfRequested();

    Vk_FrameData& frameData = myFrameCtx.myFrameDatas[ myFrameCtx.myCurrentFrame ];
    aOut.myFrameData        = &frameData;

    const auto fenceWaitStart = std::chrono::high_resolution_clock::now();
    vkWaitForFences( myRhi.myDeviceCtx.myDevice, 1, &frameData.myRenderFence, VK_TRUE, UINT64_MAX );
    aOut.myGpuFenceWaitMs = ElapsedMs( fenceWaitStart, std::chrono::high_resolution_clock::now() );

    if ( !Vk_SwapchainHost::AcquireNextImage( *this, frameData, aOut.myImageIndex ) ) {
        return false;
    }

    myFrameStats.ResetPerFrameCounters();

    const uint32_t activeViewCount = std::min( aViewCount, kGfxMaxRenderViews );
    aOut.myActiveViewCount         = activeViewCount;

    uint32_t       totalOpaqueDraws   = 0;
    uint32_t       totalTransDraws    = 0;
    uint32_t       totalOpaqueRuns    = 0;
    uint32_t       totalTransRuns     = 0;
    const uint32_t slabPartitionCount = std::max( 1u, activeViewCount );
    const uint32_t perViewMaxEntries  = std::max( 1u, VkDescriptorPolicy::kMaxInstanceSlabEntries / slabPartitionCount );
    const uint32_t perViewEntitySlots = std::max( 1u, VkDescriptorPolicy::kMaxEntitySlots / slabPartitionCount );

    aOut.mySceneSlotCount = static_cast< uint32_t >( aInput.myScene->GetSlotCount() );
    for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
        Gfx_GpuCullPushConstants& cullParams = aOut.myGpuCullViews[ viewIndex ];
        cullParams.viewProj                  = aViews[ viewIndex ].myCamera.myProj * aViews[ viewIndex ].myCamera.myView;
        cullParams.viewLayerMask             = aViews[ viewIndex ].myView.myLayerMask;
        cullParams.slotCount                 = aOut.mySceneSlotCount;
        cullParams.outputBaseSlot            = viewIndex * perViewEntitySlots;
        cullParams.pad                       = 0;
    }

    const bool lodEnabled = aToggles.myLodEnabled;

    Gfx_EntityRecordLodParams entityLodParams{};
    entityLodParams.myLodEnabled = lodEnabled;
    if ( lodEnabled && activeViewCount > 0 ) {
        entityLodParams.myCameraEye = aViews[ 0 ].myCamera.myEye;
        entityLodParams.myLodTable  = aInput.myLodTable;
        entityLodParams.myLodState  = aInput.myLodState;
    }

    if ( !mySceneGpuCtx.myDrawPrep.FillEntityRecords( *aInput.myScene, mySceneGpuCtx.myResourceTables, entityLodParams, myFrameCtx.myCurrentFrame,
                                                      myFrameCtx.myFrameDatas ) ) {
        return false;
    }

    for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
        Gfx_LodState  secondaryViewLodState;
        Gfx_LodState* lodStateForView = aInput.myLodState;
        if ( viewIndex > 0 ) {
            secondaryViewLodState = *aInput.myLodState;
            lodStateForView       = &secondaryViewLodState;
        }

        Vk_FrameDrawPrepBuildParams prepParams{};
        prepParams.myScene                  = aInput.myScene;
        prepParams.myCamera                 = &aViews[ viewIndex ].myCamera;
        prepParams.myLodTable               = aInput.myLodTable;
        prepParams.myLodState               = lodStateForView;
        prepParams.myLodEnabled             = lodEnabled;
        prepParams.myLodDebugLogicalMeshId  = aInput.myLodDebugLogicalMeshId;
        prepParams.myCurrentFrame           = myFrameCtx.myCurrentFrame;
        prepParams.myFrameDatas             = &myFrameCtx.myFrameDatas;
        prepParams.myInstanceSlabStride     = InstanceSlabStride();
        prepParams.myInstanceSlabBaseOffset = static_cast< size_t >( viewIndex ) * static_cast< size_t >( perViewMaxEntries ) * InstanceSlabStride();
        prepParams.myInstanceSlabMaxEntries = perViewMaxEntries;
        prepParams.myDrawBufferBaseIndex    = viewIndex * perViewMaxEntries;
        prepParams.myDrawBufferMaxEntries   = perViewMaxEntries;
        prepParams.myViewLayerMask          = aViews[ viewIndex ].myView.myLayerMask;
        prepParams.myGpuCullEnabled         = EngineConfig().GetGpuCullEnabled();
        prepParams.myResourceTables         = &mySceneGpuCtx.myResourceTables;

        mySceneGpuCtx.myDrawPrep.ClearFrameOutputs();
        // Fail-closed on slab overflow (same post-acquire contract as swapchain acquire failure).
        if ( !mySceneGpuCtx.myDrawPrep.UploadFromPacket( prepParams, aViewPackets[ viewIndex ] ) ) {
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
    myFrameStats.SetDrawStreamMetrics( static_cast< uint32_t >( aInput.myScene->GetActiveCount() ), totalOpaqueDraws, totalTransDraws, totalOpaqueRuns, totalTransRuns );

    const uint32_t visibleDrawsForPerf = totalOpaqueDraws + totalTransDraws;
    UtilPerfLog::AppendFrame( EngineConfig(), static_cast< uint64_t >( myFrameCtx.myFrameNumber ), myFrameStats.myFrameMs, myFrameStats.myDrawCalls, visibleDrawsForPerf,
                              aOut.myActiveViewCount, Vk_RenderMaterialPathName( myRhi.myDeviceCtx.myMaterialPath ) );

    if ( !myMaterialBindLoggedOnce ) {
        if ( myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
            UtilLogger::Info( "DESCRIPTOR", "Set 1 bindless material set (once per pass); per-draw materialIndex in Set 2 slab." );
        }
        else {
            UtilLogger::Info( "DESCRIPTOR", "Set 1 material binds this frame will be <= batch runs (" + std::to_string( totalOpaqueRuns + totalTransRuns ) + ")" );
        }
        myMaterialBindLoggedOnce = true;
    }

    if ( !myBindlessLoggedOnce && myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        UtilLogger::Debug( "BINDLESS", "recording with materialTableGeneration=" + std::to_string( mySceneGpuCtx.myResourceTables.GetMaterialTableGeneration() )
                                           + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds ) );
        myBindlessLoggedOnce = true;
    }

    aOut.myOk = true;
    return true;
}

Vk_FrameResult Vk_Renderer::DrawFrameGpu( const Gfx_FrameDebugToggles& aToggles, Vk_FrameCpuPrepResult& aPrep ) {
    ( void )aToggles;
    if ( !aPrep.myOk || aPrep.myFrameData == nullptr ) {
        return Vk_FrameResult::SkipFrame;
    }

    Vk_FrameData& frameData = *aPrep.myFrameData;

    // Env UBO after debug panels patch myEnvironmentData; camera slices already in PrepareFrameCpu.
    // LightingGlobals uploaded after shadow pass (or before resolve when shadows off).
    Vk_FrameUniformUploader::UpdateEnvironment( *this, myFrameCtx.myCurrentFrame );

    vkResetFences( myRhi.myDeviceCtx.myDevice, 1, &frameData.myRenderFence );
    vkResetCommandBuffer( frameData.myCommandBuffer, 0 );

    const VkCommandBufferBeginInfo beginInfo = VkInit::CommandBufferBeginInfo( 0 );
    if ( vkBeginCommandBuffer( frameData.myCommandBuffer, &beginInfo ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to begin recording command buffer!" );
    }

    Vk_GpuCull::RecordDispatches( *this, frameData.myCommandBuffer, aPrep );

    Vk_RendererContexts contexts = BuildContexts();
    Vk_ScenePasses::RecordScene( contexts, aToggles, frameData.myCommandBuffer, aPrep.myImageIndex, aPrep.myViewports, aPrep.myScissors, aPrep.myFrameDescriptors,
                                 aPrep.myActiveViewCount, aPrep.myViewPackets );

    ImGui::Render();
    Vk_ScenePasses::RecordImGui( contexts, frameData.myCommandBuffer, aPrep.myImageIndex );

    if ( vkEndCommandBuffer( frameData.myCommandBuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to record command buffer!" );
    }

    // Wall-clock breakdown for overlay: Work ends before present; PresentWait measured inside SubmitAndPresent.
    // Committed on next BeginPlatformFrame via SetPendingFrameBreakdown + PushFrameTime (same ring index as Frame ms).
    const auto           workEnd       = std::chrono::high_resolution_clock::now();
    float                presentWaitMs = 0.f;
    const Vk_FrameResult frameResult   = Vk_SwapchainHost::SubmitAndPresent( *this, frameData, aPrep.myImageIndex, &presentWaitMs );
    const float          workMs        = ElapsedMs( myPlatformCtx.myLastFrameTime, workEnd );
    myFrameStats.SetPendingFrameBreakdown( workMs, presentWaitMs, aPrep.myGpuFenceWaitMs, myVsync );

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

void Vk_Renderer::CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const {
    myPlatformCtx.myRenderDoc.CmdBeginDebugLabel( aCommandBuffer, aLabelName );
}

void Vk_Renderer::CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const {
    myPlatformCtx.myRenderDoc.CmdEndDebugLabel( aCommandBuffer );
}

bool Vk_Renderer::AreCommandDebugLabelsEnabled() const {
    return myPlatformCtx.myRenderDoc.AreCommandLabelsEnabled();
}

Vk_RendererContexts Vk_Renderer::BuildContexts() {
    return Vk_RendererContexts{ *this };
}

void Vk_Renderer::LogM1PerfSnapshot() const {
    const uint32_t visibleDraws = myFrameStats.myVisibleOpaqueDraws + myFrameStats.myVisibleTransparentDraws;
    const uint32_t batchRuns    = myFrameStats.myOpaqueBatchRuns + myFrameStats.myTransparentBatchRuns;
    UtilLogger::Info( "PERF", "frameMs=" + std::to_string( myFrameStats.myFrameMs ) + " fps=" + std::to_string( myFrameStats.myFps )
                                  + " entities=" + std::to_string( myFrameStats.myActiveEntities ) + " visibleDraws=" + std::to_string( visibleDraws )
                                  + " batchRuns=" + std::to_string( batchRuns ) + " (opaque=" + std::to_string( myFrameStats.myOpaqueBatchRuns ) + " transparent="
                                  + std::to_string( myFrameStats.myTransparentBatchRuns ) + ")" + " materialSetBinds=" + std::to_string( myFrameStats.myMaterialSetBinds )
                                  + " pipelineBinds=" + std::to_string( myFrameStats.myPipelineBinds ) + " drawCalls=" + std::to_string( myFrameStats.myDrawCalls )
                                  + " materialPath=" + Vk_RenderMaterialPathName( myRhi.myDeviceCtx.myMaterialPath ) );
}

void Vk_Renderer::RefreshMaterialPipelinesAfterSwapchainRecreate() {
    const VkPipeline opaquePipe = myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBasicPipelineBindless : mySceneGpuCtx.myBasicPipeline;
    const VkPipeline transPipe =
        myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myTransparentPipelineBindless : mySceneGpuCtx.myTransparentPipeline;
    const VkPipelineLayout layout =
        myRhi.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? mySceneGpuCtx.myBindlessPipelineLayout : mySceneGpuCtx.myPipelineLayout;
    mySceneGpuCtx.myResourceTables.RefreshMaterialPipelines( opaquePipe, transPipe, layout );
}

void Vk_Renderer::InitVk_QueueFamilyIndices() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( myRhi.myDeviceCtx.myPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( myRhi.myDeviceCtx.myPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( myRhi.myDeviceCtx.myPhysicalDevice, i, myRhi.myDeviceCtx.mySurface, &presentSupport );
        if ( presentSupport && ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myRhi.myDeviceCtx.myQueueFamilyIndices.myPresentFamily  = i;
            myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily = i;
        }

        // Prefer a dedicated transfer-only family when available (staging uploads).
        if ( ( queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT ) && !( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) ) {
            myRhi.myDeviceCtx.myQueueFamilyIndices.myTransferFamily = i;
        }

        if ( myRhi.myDeviceCtx.myQueueFamilyIndices.isComplete() )
            break;

        i++;
    }

    myRhi.myDeviceCtx.myQueueFamilyIndices.ApplyTransferFallback();

    const uint32_t graphicsFamily = myRhi.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value_or( 0 );
    const uint32_t transferFamily = myRhi.myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value_or( graphicsFamily );
    const bool     useConcurrent  = graphicsFamily != transferFamily;
    // Startup signal for queue-family ownership policy used by image/buffer allocations.
    UtilLogger::Info( "VULKAN", "Queue families: graphics=" + std::to_string( graphicsFamily ) + " transfer=" + std::to_string( transferFamily )
                                    + " (image/buffer sharing=" + std::string( useConcurrent ? "CONCURRENT" : "EXCLUSIVE" ) + ")" );
}

#pragma region Helpers - device, queues, shaders

void Vk_Renderer::CheckExtensionSupport() const {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > extensions( extensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );

    UtilLogger::Debug( "VULKAN", "Instance extension discovery (" + std::to_string( extensionCount ) + "):" );
    for ( const VkExtensionProperties& extension : extensions ) {
        UtilLogger::Debug( "VULKAN", std::string( "  " ) + extension.extensionName );
    }
}

bool Vk_Renderer::CheckValidationLayerSupport() const {
    return UtilValidationLayers::AreLayersAvailable( myRhi.myDeviceCtx.myValidationLayers );
}

void Vk_Renderer::SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers ) {
    myRhi.SetEnableValidationLayers( aEnableValidationLayers, std::move( someValidationLayers ) );
}

void Vk_Renderer::SetRequiredExtension( std::vector< const char* > someDeviceExtensions ) {
    myRhi.SetRequiredExtension( std::move( someDeviceExtensions ) );
}

bool Vk_Renderer::CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const {
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &myRhi.myDeviceCtx.myPhysicalDeviceProperties );

    vkGetPhysicalDeviceFeatures( aPhysicalDevice, &myRhi.myDeviceCtx.myPhysicalDeviceFeatures );

    Vk_QueueFamilyIndices indices = FindQueueFamilies( aPhysicalDevice );

    bool extensionSupported = CheckExtensionSupport( aPhysicalDevice );

    // Swapchain formats + present modes are required for renderable output.
    bool swapChainAdequate = false;
    if ( extensionSupported ) {
        Vk_SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport( aPhysicalDevice );
        swapChainAdequate                           = !swapChainSupport.myFormats.empty() && !swapChainSupport.myPresentModes.empty();
    }

#ifdef _DEBUG
    UtilLogger::Debug( "GPU", "minUniformBufferOffsetAlignment=" + std::to_string( myRhi.myDeviceCtx.myPhysicalDeviceProperties.limits.minUniformBufferOffsetAlignment ) );
#endif  // _DEBUG

    return indices.isComplete() && extensionSupported && swapChainAdequate && myRhi.myDeviceCtx.myPhysicalDeviceFeatures.samplerAnisotropy;
}

Vk_QueueFamilyIndices Vk_Renderer::FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const {
    Vk_QueueFamilyIndices indices;
    uint32_t              queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, nullptr );

    std::vector< VkQueueFamilyProperties > queueFamilies( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( aPhysicalDevice, &queueFamilyCount, queueFamilies.data() );

    int i = 0;
    for ( const auto& queueFamily : queueFamilies ) {
        // Prefer one family that supports both graphics + present.
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR( aPhysicalDevice, i, myRhi.myDeviceCtx.mySurface, &presentSupport );
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

Vk_SwapChainSupportDetails Vk_Renderer::QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const {
    Vk_SwapChainSupportDetails details;

    // Surface capabilities (extent/image count transform limits).
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( aPhysicalDevice, myRhi.myDeviceCtx.mySurface, &details.myCapabilities );

    // Supported color formats/colorspaces for present images.
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, myRhi.myDeviceCtx.mySurface, &formatCount, nullptr );

    if ( formatCount != 0 ) {
        details.myFormats.resize( formatCount );
        vkGetPhysicalDeviceSurfaceFormatsKHR( aPhysicalDevice, myRhi.myDeviceCtx.mySurface, &formatCount, details.myFormats.data() );
    }

    // Supported present modes (FIFO/MAILBOX/IMMEDIATE).
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, myRhi.myDeviceCtx.mySurface, &presentModeCount, nullptr );

    if ( presentModeCount != 0 ) {
        details.myPresentModes.resize( presentModeCount );
        vkGetPhysicalDeviceSurfacePresentModesKHR( aPhysicalDevice, myRhi.myDeviceCtx.mySurface, &presentModeCount, details.myPresentModes.data() );
    }

    return details;
}

bool Vk_Renderer::CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > availableExtensions( extensionCount );
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, availableExtensions.data() );

    std::set< std::string > requiredExtensions( myRhi.myDeviceCtx.myDeviceExtensions.begin(), myRhi.myDeviceCtx.myDeviceExtensions.end() );

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

VkSurfaceFormatKHR Vk_Renderer::ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) const {
    for ( const auto& availableFormat : someAvailableFormats ) {
        if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
            return availableFormat;
        }
    }

    return someAvailableFormats[ 0 ];
}

VkPresentModeKHR Vk_Renderer::ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) const {
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

VkExtent2D Vk_Renderer::ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) const {
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

VkShaderModule Vk_Renderer::CreateShaderModule( const std::vector< char >& someShaderCode ) const {
    return myRhi.CreateShaderModule( someShaderCode );
}

VkShaderModule Vk_Renderer::CreateShaderModule( const std::string aShaderPath ) const {
    UtilLogger::Debug( "SHADER", "Loading shader module: " + aShaderPath );
    const auto shaderCode = UtilLoader::ReadFile( EngineConfig(), aShaderPath );

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode    = reinterpret_cast< const uint32_t* >( shaderCode.data() );

    VkShaderModule shaderModule;
    const VkResult moduleResult = vkCreateShaderModule( myRhi.myDeviceCtx.myDevice, &createInfo, nullptr, &shaderModule );
    if ( moduleResult != VK_SUCCESS ) {
        UtilLogger::Error( "SHADER", "vkCreateShaderModule " + UtilVulkanResult::Describe( moduleResult ) + " path=" + aShaderPath
                                         + " codeSize=" + std::to_string( shaderCode.size() ) );
    }
    UtilVulkanResult::ThrowOnFailure( moduleResult, "vkCreateShaderModule" );

    return shaderModule;
}

// Required when bound pipeline declares dynamic viewport/scissor/line width (SetDefaultDynamicStates).
void Vk_Renderer::SetGraphicsDynamicState( VkCommandBuffer aCommandBuffer, const VkViewport& aViewport, const VkRect2D& aScissor ) const {
    // CONTRACT: VkDynamicState list must match Vk_PipelineBuilder::SetDefaultDynamicStates().
    vkCmdSetViewport( aCommandBuffer, 0, 1, &aViewport );
    vkCmdSetScissor( aCommandBuffer, 0, 1, &aScissor );

    vkCmdSetLineWidth( aCommandBuffer, 1.0f );
}

uint32_t Vk_Renderer::FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties( myRhi.myDeviceCtx.myPhysicalDevice, &memProperties );

    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ ) {
        if ( ( aTypeFiler & ( 1 << i ) ) && ( memProperties.memoryTypes[ i ].propertyFlags & someProperties ) == someProperties ) {
            return i;
        }
    }

    throw std::runtime_error( "failed to find suitable memory type!" );
}

void Vk_Renderer::CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool isExclusive ) const {
    myRhi.CreateBuffer( aSize, aBufferUsage, aMemoryUsage, aBuffer, isExclusive );
}

void Vk_Renderer::CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myRhi.CopyBuffer( aSrcBuffer, aDstBuffer, aSize );
}

void Vk_Renderer::CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const {
    myRhi.CopyBufferOnGraphicsQueue( aSrcBuffer, aDstBuffer, aSize );
}

size_t Vk_Renderer::InstanceSlabStride() const {
    return PadUniformBufferSize( sizeof( GpuObjectData ) );
}

// Per-frame UBO upload is delegated to Vk_FrameUniformUploader.
void Vk_Renderer::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                               Vk_AllocatedImage& anImage ) const {
    myRhi.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, anImage );
}

void Vk_Renderer::CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                               VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myRhi.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Renderer::CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                               VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const {
    myRhi.CreateImage( anExtent, aFormat, aTiling, anImageUsage, aMemoryUsage, aMipLevel, aNumSamples, anImage );
}

void Vk_Renderer::TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const {
    myRhi.TransitionImageLayout( aImage, aFormat, anOldLayout, aNewLayout, aMipLevel );
}

void Vk_Renderer::CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const {
    myRhi.CopyBufferToImage( aBuffer, aImage, aWidth, aHeight );
}

void Vk_Renderer::GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const {
    myRhi.GenerateMipmaps( aImage, aImageFormat, aTexWidth, aTexHeight, aMipLevel );
}

VkImageView Vk_Renderer::CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel ) const {
    return myRhi.CreateImageView( anImage, aFormat, anAspect, aMipLevel );
}

VkFormat Vk_Renderer::FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const {
    return myRhi.FindSupportedFormat( someCandidates, aTiling, someFeatures );
}

VkFormat Vk_Renderer::FindDepthFormat() const {
    return myRhi.FindDepthFormat();
}

bool Vk_Renderer::HasStencilComponent( VkFormat aFormat ) const {
    return myRhi.HasStencilComponent( aFormat );
}

VkSampleCountFlagBits Vk_Renderer::GetMaxUsableSampleCount() const {
    return myRhi.GetMaxUsableSampleCount();
}

void Vk_Renderer::BeginImGuiFrame() {
    myPlatformCtx.myImGuiLayer.NewFrame();
}

void Vk_Renderer::OnPlatformFrameStart( std::chrono::high_resolution_clock::time_point aFrameStart, float aDeltaSeconds ) {
    if ( myPlatformCtx.myHasLastFrameTime ) {
        myFrameStats.PushFrameTime( aDeltaSeconds * 1000.f );
    }
    myPlatformCtx.myLastFrameTime    = aFrameStart;
    myPlatformCtx.myHasLastFrameTime = true;
}

void Vk_Renderer::ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput, const Util_CameraSettings& aCameraSettings ) {
    myCamera.ApplyInput( aDeltaSeconds, aInput, aCameraSettings );
}

void Vk_Renderer::SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime ) {
    myPlatformCtx.myFrameInputSampleTime    = aSampleTime;
    myPlatformCtx.myHasFrameInputSampleTime = true;
}

size_t Vk_Renderer::PadUniformBufferSize( size_t anOriginalSize ) const {
    return myRhi.PadUniformBufferSize( anOriginalSize );
}

void Vk_Renderer::SetPlatformWindow( GLFWwindow* aWindow ) {
    myPlatformCtx.myWindow = aWindow;
}

void Vk_Renderer::NotifyFramebufferResized() {
    mySwapchainCtx.myFramebufferResized = true;
}

void Vk_Renderer::BindPlatformHost( App_PlatformHost* aPlatformHost ) {
    myPlatformHost = aPlatformHost;
}

#pragma endregion

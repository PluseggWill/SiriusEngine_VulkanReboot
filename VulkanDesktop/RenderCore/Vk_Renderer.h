#pragma once

#include <array>

#include <chrono>

#include <string>

#include <vk_mem_alloc.h>

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_Bounds.h"
#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_FramePrepInput.h"
#include "../Gfx/Gfx_LightingGlobals.h"
#include "../Gfx/Gfx_PostSettings.h"
#include "../Gfx/Gfx_RenderView.h"
#include "../Gfx/Gfx_SceneGpuLoadParams.h"

#include "../Util/Util_FrameStats.h"

#include "../Util/Util_InputSnapshot.h"

#include "Vk_ActiveRenderView.h"

#include "Vk_Camera.h"

#include "Vk_DataStruct.h"

#include "Vk_DeviceContext.h"

#include "Vk_FrameContext.h"

#include "Vk_FrameCpuPrepResult.h"

#include "Vk_FrameResult.h"

#include "Vk_AoPass.h"
#include "Vk_ClusterBuildPass.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_FrameDrawPrep.h"
#include "Vk_FrameGraphBuilder.h"
#include "Vk_GBufferPass.h"
#include "Vk_GpuCull.h"
#include "Vk_IblResources.h"
#include "Vk_PostProcessPass.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_ShadowMapPass.h"

#include "Vk_PlatformContext.h"

#include "Vk_ResourceContext.h"
#include "Vk_RhiDevice.h"
#include "Vk_RendererContexts.h"

#include "Vk_SceneGpuContext.h"

#include "Vk_SwapchainContext.h"

#include <optional>

#include "Vk_FrameLimits.h"

constexpr bool FILL_MODE_LINE = false;  // debug wireframe via polygon mode

struct Vk_AllocatedImage;

struct Vk_AllocatedBuffer;

struct GLFWwindow;

struct Util_EngineConfig;
class App_PlatformHost;

class Vk_Renderer;

// RHI-shaped Vulkan backend: device, swapchain, pipelines, descriptors, frame sync, command record/submit.

// Peel slices read Vk_*Context members on Vk_Renderer (no friend access); orchestration stays in Vk_*Host modules.

class Vk_Renderer {

public:
    Vk_Renderer();

    ~Vk_Renderer();

    Vk_Renderer( const Vk_Renderer& ) = delete;

    Vk_Renderer& operator=( const Vk_Renderer& ) = delete;

    // Non-owning; must outlive Vk_Renderer (Application::myConfig). Call from InitApp before RenderCore reads config.
    void BindEngineConfig( const Util_EngineConfig* aConfig );

    const Util_EngineConfig& EngineConfig() const;

    void SetSize( const uint32_t aWidth, const uint32_t aHeight );

    void SetVsync( bool aVsync );

    void Reset();

    void InitRenderDevice();

    // Application must call App_LoadSceneCpuState first and pass scene DTO from App layer.
    void LoadSceneGpuResources( const Gfx_SceneGpuLoadParams& aLoadParams );

    void UnloadSceneGpuResources();

    const std::string& GetLoadedSceneLogicalPath() const;

    bool HasLoadedScene() const;

    Vk_DeletionQueue& GetSceneDeletionQueue() {

        return mySceneGpuCtx.mySceneDeletionQueue;
    }

    void Shutdown();

    void BeginImGuiFrame();  // after InputSystem::Sample; see Vk_PlatformFrame.h
    void OnPlatformFrameStart( std::chrono::high_resolution_clock::time_point aFrameStart, float aDeltaSeconds );

    void ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput, const Util_CameraSettings& aCameraSettings );

    void SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime );

    // aViews built by Application (BuildActiveRenderViews); core does not read scene JSON for PiP here.

    bool PrepareFrameCpu( const Gfx_FramePrepInput& aInput, const Gfx_FrameDebugToggles& aToggles, const std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews,
                          uint32_t aViewCount, const std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews >& aViewPackets, Vk_FrameCpuPrepResult& aOut );

    Vk_FrameResult DrawFrameGpu( const Gfx_FrameDebugToggles& aToggles, Vk_FrameCpuPrepResult& aPrep );

    GpuEnvironmentData& GetEnvironmentData() {

        return myEnvironmentData;
    }

    Gfx_LightingSettings& GetLightingSettings() {

        return myLightingSettings;
    }

    Gfx_AoSettings& GetAoSettings() {

        return myAoSettings;
    }

    Gfx_PostSettings& GetPostSettings() {

        return myPostSettings;
    }

    void ConfigureRenderDoc( bool aEnableRenderDoc );

    void TriggerRenderDocCapture();

    bool IsRenderDocEnabled() const;

    void CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const;

    void CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const;

    // VK_EXT_debug_utils labels loaded (--renderdoc + extension); used to skip label formatting on hot path.
    bool AreCommandDebugLabelsEnabled() const;
    Vk_RendererContexts BuildContexts();

    void SetPlatformWindow( GLFWwindow* aWindow );
    void NotifyFramebufferResized();
    void BindPlatformHost( App_PlatformHost* aPlatformHost );

    const Vk_Camera& GetFlyCamera() const {

        return myCamera;
    }

    Gfx_Bounds GetShadowCasterBounds() const;

    VkExtent2D GetSwapChainExtent() const {

        return mySwapchainCtx.mySwapChainExtent;
    }

    void SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers );

    void SetRequiredExtension( std::vector< const char* > someDeviceExtensions );

    void CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool isExclusive ) const;

    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,

                      Vk_AllocatedImage& anImage ) const;

    void CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,

                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;

    void CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,

                      VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;

    VkShaderModule CreateShaderModule( const std::vector< char >& someShaderCode ) const;

    VkShaderModule CreateShaderModule( const std::string aShaderPath ) const;

    VkImageView CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;

    void TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;

    void CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;

    void GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;

    void CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;

    bool HasStencilComponent( VkFormat aFormat ) const;

    // Exposed for Vk_RenderDevice / Vk_SwapchainHost (replaces former friend access).

    void CreateInstance();

    void CreateSurface();
    void RecreateSurface();

    const Vk_ResourceContext& GetResourceContext() const {
        return myRhi.myResourceContext;
    }

    Vk_RhiDevice& Rhi() {
        return myRhi;
    }

    const Vk_RhiDevice& Rhi() const {
        return myRhi;
    }

    void PickPhysicalDevice();

    void InitVk_QueueFamilyIndices();

    void CreateLogicalDevice();

    void CreateCommandPool();

    void InitAllocator();

    Vk_SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const;

    VkSurfaceFormatKHR ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) const;

    VkPresentModeKHR ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) const;

    VkExtent2D ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) const;

    VkFormat FindDepthFormat() const;

    size_t PadUniformBufferSize( size_t anOriginalSize ) const;

    void RefreshMaterialPipelinesAfterSwapchainRecreate();

    void SetGraphicsDynamicState( VkCommandBuffer aCommandBuffer, const VkViewport& aViewport, const VkRect2D& aScissor ) const;

    bool myVsync = false;

    Util_FrameStats myFrameStats;

    Vk_RhiDevice myRhi;

    Vk_SwapchainContext mySwapchainCtx;

    Vk_FrameContext myFrameCtx;

    Vk_SceneGpuContext mySceneGpuCtx;

    Vk_GBufferState myGBufferState;

    Vk_ClusterBuildState myClusterBuildState;

    Vk_DeferredLightingState myDeferredLightingState;

    Vk_DepthPyramidState myDepthPyramidState;

    Vk_AoState           myAoState;
    Vk_ShadowAoSoftState myShadowAoSoftState;

    Vk_PostProcessState myPostProcessState;

    Vk_IblResourcesState myIblResourcesState;

    Vk_ShadowMapState myShadowMapState;

    Vk_GpuCullState myGpuCullState;

    Vk_PlatformContext myPlatformCtx;

    // Session presentation (fly camera + env UBO); not moved into contexts yet.
    Vk_Camera myCamera;

    GpuEnvironmentData myEnvironmentData;

    Gfx_LightingSettings myLightingSettings;

    Gfx_AoSettings myAoSettings;

    Gfx_PostSettings myPostSettings;

    Vk_AllocatedBuffer myEnvDataBuffer;

    Vk_AllocatedBuffer myLightingGlobalsBuffer;

    bool myMaterialBindLoggedOnce = false;

    bool myBindlessLoggedOnce = false;

    bool myM1PerfLoggedOnce = false;

    bool myPrepareFrameSlabOverflowLogged = false;

    bool mySceneGpuLoaded = false;

    std::string myLoadedSceneLogicalPath;

    const Gfx_SceneSoA* myBoundSceneSoA = nullptr;

    const Util_EngineConfig* myEngineConfig = nullptr;
    App_PlatformHost* myPlatformHost = nullptr;

private:
    void Clear();

    void SyncResourceContext();

    void CreateFrameData();

    void CreateInstanceSlabs();

    void CreateDrawTemplateBuffers();

    void CreateEntityRecordBuffers();

    void CreateUniformBuffers();

    void InitImGui();

    void ShutdownImGui();

    void LogM1PerfSnapshot() const;

    size_t InstanceSlabStride() const;

    void CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;

    void CheckExtensionSupport() const;

    bool CheckValidationLayerSupport() const;

    bool CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const;

    bool CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const;

    Vk_QueueFamilyIndices FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const;

    uint32_t FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const;

    VkFormat FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const;

    VkSampleCountFlagBits GetMaxUsableSampleCount() const;
};

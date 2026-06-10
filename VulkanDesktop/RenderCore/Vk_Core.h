#pragma once

#include <array>

#include <chrono>

#include <string>

#include <vk_mem_alloc.h>

#include "../Gfx/Gfx_RenderView.h"

#include "../Util/Util_FrameStats.h"

#include "../Util/Util_InputSnapshot.h"

#include "Vk_ActiveRenderView.h"

#include "Vk_Camera.h"

#include "Vk_DataStruct.h"

#include "Vk_DeviceContext.h"

#include "Vk_FrameContext.h"

#include "Vk_FrameCpuPrepResult.h"

#include "Vk_FrameResult.h"

#include "Vk_FrameDrawPrep.h"

#include "Vk_PlatformContext.h"

#include "Vk_ResourceContext.h"

#include "Vk_SceneGpuContext.h"

#include "Vk_SwapchainContext.h"

#include <optional>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;  // swapchain frames in flight; also env UBO slice count

constexpr bool FILL_MODE_LINE = false;  // debug wireframe via polygon mode

struct Vk_AllocatedImage;

struct Vk_AllocatedBuffer;

struct Gfx_Texture;

struct Gfx_Material;

class Gfx_Vertex;

class Gfx_Mesh;

class Gfx_RenderObject;

struct GLFWwindow;

struct WorldState;

struct DebugUIState;

struct Gfx_SceneDesc;

struct Util_EngineConfig;

class Vk_Core;

// RHI-shaped Vulkan backend: device, swapchain, pipelines, descriptors, frame sync, command record/submit.

// Peel slices read Vk_*Context members on Vk_Core (no friend access); orchestration stays in Vk_*Host modules.

class Vk_Core {

public:
    static Vk_Core& GetInstance();

    Vk_Core( const Vk_Core& ) = delete;

    Vk_Core& operator=( const Vk_Core& ) = delete;

    void BindWorldState( WorldState* aWorld );

    void BindDebugUI( DebugUIState* aDebugUI );

    // Non-owning; must outlive Vk_Core (Application::myConfig). Call from InitApp before RenderCore reads config.
    void BindEngineConfig( const Util_EngineConfig* aConfig );

    const Util_EngineConfig& EngineConfig() const;

    void SetSize( const uint32_t aWidth, const uint32_t aHeight );

    void SetVsync( bool aVsync );

    void Reset();

    void InitWindow();

    void InitRenderDevice();

    void LoadSceneResources( Gfx_SceneDesc aScene, std::string aLogicalScenePath = {} );

    void UnloadScene();

    const std::string& GetLoadedSceneLogicalPath() const;

    bool HasLoadedScene() const;

    Vk_DeletionQueue& GetSceneDeletionQueue() {

        return mySceneGpuCtx.mySceneDeletionQueue;
    }

    void Shutdown();

    void BeginPlatformFrame( float& aOutDeltaSeconds );

    void ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput );

    void SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime );

    // aViews built by Application (BuildActiveRenderViews); core does not read scene JSON for PiP here.

    bool PrepareFrameCpu( WorldState& aWorld, const std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews, uint32_t aViewCount, Vk_FrameCpuPrepResult& aOut );

    Vk_FrameResult DrawFrameGpu( const DebugUIState& aDebugUI, Vk_FrameCpuPrepResult& aPrep );

    GpuEnvironmentData& GetEnvironmentData() {

        return myEnvironmentData;
    }

    void ConfigureRenderDoc( bool aEnableRenderDoc );

    void TriggerRenderDocCapture();

    bool IsRenderDocEnabled() const;

    void CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const;

    void CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const;

    // VK_EXT_debug_utils labels loaded (--renderdoc + extension); used to skip label formatting on hot path.
    bool AreCommandDebugLabelsEnabled() const;

    bool ShouldClose() const;

    GLFWwindow* GetWindow() const {

        return myPlatformCtx.myWindow;
    }

    const Vk_Camera& GetFlyCamera() const {

        return myCamera;
    }

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

    // SURFACE_LOST recovery: destroy and recreate WSI surface (window unchanged).
    void RecreateSurface();

    const Vk_ResourceContext& GetResourceContext() const {
        return myResourceContext;
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

    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

    bool myVsync = true;

    Util_FrameStats myFrameStats;

    Vk_DeviceContext myDeviceCtx;

    Vk_SwapchainContext mySwapchainCtx;

    Vk_FrameContext myFrameCtx;

    Vk_SceneGpuContext mySceneGpuCtx;

    Vk_PlatformContext myPlatformCtx;

    // Session presentation (fly camera + env UBO); not moved into contexts yet.

    Vk_Camera myCamera;

    GpuEnvironmentData myEnvironmentData;

    Vk_AllocatedBuffer myEnvDataBuffer;

    Vk_ResourceContext myResourceContext;

    bool myMaterialBindLoggedOnce = false;

    bool myBindlessLoggedOnce = false;

    bool myM1PerfLoggedOnce = false;

    bool myPrepareFrameSlabOverflowLogged = false;

private:
    Vk_Core();

    ~Vk_Core();

    void Clear();

    void SyncResourceContext();

    void CreateFrameData();

    void CreateInstanceSlabs();

    void CreateDrawTemplateBuffers();

    void CreateEntityRecordBuffers();

    void CreateUniformBuffers();

    void InitImGui();

    void ShutdownImGui();

    WorldState& World() const;

    DebugUIState& DebugUI() const;

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

    WorldState* myWorld = nullptr;

    DebugUIState* myDebugUI = nullptr;

    const Util_EngineConfig* myEngineConfig = nullptr;
};

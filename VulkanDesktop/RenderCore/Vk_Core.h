#pragma once
#include <array>
#include <chrono>
#include <string>
#include <vk_mem_alloc.h>

#include "../Gfx/Gfx_RenderView.h"
#include "../Util/Util_FrameStats.h"
#include "../Util/Util_ImGuiLayer.h"
#include "../Util/Util_InputSnapshot.h"
#include "../Util/Util_RenderDebugPanel.h"
#include "../Util/Util_ScenePanel.h"
#include "Vk_Bindless.h"
#include "Vk_Camera.h"
#include "Vk_DataStruct.h"
#include "Vk_Enum.h"
#include "Vk_FrameData.h"
#include "Vk_FrameDrawPrep.h"
#include "Vk_RenderDoc.h"
#include "Vk_ResourceContext.h"
#include "Vk_ResourceTables.h"
#include <optional>

constexpr int  MAX_FRAMES_IN_FLIGHT = 2;      // swapchain frames in flight; also env UBO slice count
constexpr bool FILL_MODE_LINE       = false;  // debug wireframe via polygon mode

struct Vk_AllocatedImage;
struct Vk_AllocatedBuffer;
struct Gfx_Texture;
struct Gfx_Material;
class Gfx_Vertex;
class Gfx_Mesh;
class Gfx_RenderObject;
struct GLFWwindow;
struct WorldState;
struct Gfx_SceneDesc;

class Vk_Core;

namespace VkShaderEffectMeta {
void RunLitBatchLayoutMismatchValidationTest( Vk_Core& aCore );
}

// RHI-shaped Vulkan backend: device, swapchain, pipelines, descriptors, frame sync, command record/submit.
// Orchestration slices: Vk_RenderDevice, Vk_SwapchainHost, Vk_DescriptorSystem, Vk_GfxPipelineCache, Vk_ScenePasses,
// Vk_FrameDrawPrep, Vk_SceneHost (SoA/LOD + demo camera/env defaults), Vk_PlatformFrame.
class Vk_Core {
    friend class Vk_RenderDevice;
    friend class Vk_SwapchainHost;
    friend class Vk_DescriptorSystem;
    friend void VkShaderEffectMeta::RunLitBatchLayoutMismatchValidationTest( Vk_Core& );
    friend class Vk_GfxPipelineCache;
    friend class Vk_DevicePipelineCache;
    friend class Vk_ScenePasses;
    friend class Vk_FrameUniformUploader;
    friend class Vk_SceneHost;
    friend class Vk_PlatformFrame;

public:
    struct MultiViewState {
        bool     myEnablePiP            = true;
        uint32_t mySecondaryCameraIndex = 0;
    };

    struct ActiveRenderView {
        Gfx_RenderView myView;
        Vk_Camera      myCamera;
        VkViewport     myViewport{};
        VkRect2D       myScissor{};
    };

    static Vk_Core& GetInstance();
    Vk_Core( const Vk_Core& )            = delete;
    Vk_Core& operator=( const Vk_Core& ) = delete;

    // Application-owned scene CPU (non-owning); required before LoadSceneResources / main loop.
    void BindWorldState( WorldState* aWorld );

    void SetSize( const uint32_t aWidth, const uint32_t aHeight );
    void SetVsync( bool aVsync );
    void Reset();

    // Application lifecycle (orchestrated by App/Application).
    void InitWindow();
    void InitRenderDevice();
    void LoadSceneResources( Gfx_SceneDesc aScene, std::string aLogicalScenePath = {} );
    void UnloadScene();

    const std::string& GetLoadedSceneLogicalPath() const;
    bool               HasLoadedScene() const;
    std::string TakePendingSceneReloadPath();

    // Scene-scoped GPU teardown queue (meshes, textures, descriptor pool, material buffers). Flushed in UnloadScene.
    Vk_DeletionQueue& GetSceneDeletionQueue() {
        return mySceneDeletionQueue;
    }
    void        Shutdown();
    void        BeginPlatformFrame( float& aOutDeltaSeconds );
    void        ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput );
    void        SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime );
    void        Render();
    void        ConfigureRenderDoc( bool aEnableRenderDoc );
    void        TriggerRenderDocCapture();
    bool        IsRenderDocEnabled() const;
    void        CmdBeginDebugLabel( VkCommandBuffer aCommandBuffer, const char* aLabelName ) const;
    void        CmdEndDebugLabel( VkCommandBuffer aCommandBuffer ) const;
    bool        ShouldClose() const;
    GLFWwindow* GetWindow() const {
        return myWindow;
    }

    void SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers );
    void SetRequiredExtension( std::vector< const char* > someDeviceExtensions );

    // Resource helpers (used by Gfx/Util loaders via Vk_ResourceContext injection long-term).
    // isExclusive: true = single queue family; false = graphics+transfer concurrent when families differ.
    void           CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, Vk_AllocatedBuffer& aBuffer, bool isExclusive ) const;
    void           CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                                Vk_AllocatedImage& anImage ) const;
    void           CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    void           CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, Vk_AllocatedImage& anImage ) const;
    VkShaderModule CreateShaderModule( const std::vector< char >& someShaderCode ) const;
    VkShaderModule CreateShaderModule( const std::string aShaderPath ) const;
    VkImageView    CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;
    void           TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void           CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;
    void           GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;
    void           CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    bool           HasStencilComponent( VkFormat aFormat ) const;

private:
    Vk_Core();
    ~Vk_Core();

    void Clear();
    void SyncResourceContext();

    // Device init (Vk_RenderDevice + frame slabs; env UBO buffer allocated here, CPU defaults at scene load).
    void CreateInstance();
    void PickPhysicalDevice();
    void InitVk_QueueFamilyIndices();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateCommandPool();
    void InitAllocator();
    void CreateFrameData();
    void CreateInstanceSlabs();
    void CreateUniformBuffers();
    void InitImGui();
    void ShutdownImGui();

    void   DrawFrame( const Vk_FrameData aFrameData );
    WorldState& World() const;
    void   RefreshMaterialPipelinesAfterSwapchainRecreate();
    void   LogM1PerfSnapshot() const;
    size_t InstanceSlabStride() const;
    // vkCmdSetViewport/Scissor/LineWidth for scene pipelines; call once per scene render pass after BeginRenderPass.
    void SetGraphicsDynamicState( VkCommandBuffer aCommandBuffer, const VkViewport& aViewport, const VkRect2D& aScissor ) const;
    std::array< ActiveRenderView, kGfxMaxRenderViews > BuildActiveRenderViews( uint32_t& aOutViewCount ) const;

    // Helper functions:
    void                       CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void                       CheckExtensionSupport() const;
    bool                       CheckValidationLayerSupport() const;
    bool                       CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const;
    bool                       CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const;
    Vk_QueueFamilyIndices      FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const;
    Vk_SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const;
    VkSurfaceFormatKHR         ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) const;
    VkPresentModeKHR           ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) const;
    VkExtent2D                 ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) const;
    uint32_t                   FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const;
    VkCommandBuffer            BeginSingleTimeCommands( VkCommandPool aCommandPool ) const;
    void                       EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const;
    VkFormat                   FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const;
    VkFormat                   FindDepthFormat() const;
    VkSampleCountFlagBits      GetMaxUsableSampleCount() const;
    size_t                     PadUniformBufferSize( size_t anOriginalSize ) const;

    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    bool            myVsync       = true;
    uint32_t        myFrameNumber = 0;
    Util_FrameStats myFrameStats;
    VmaAllocator    myAllocator;

#pragma region View Data Functions
    Vk_Camera           myCamera;
    Util_CameraSettings myCameraSettings;
    GpuEnvironmentData  myEnvironmentData;
    Vk_AllocatedBuffer  myEnvDataBuffer;

    Vk_ResourceContext              myResourceContext;
    Vk_ResourceTables               myResourceTables;
    std::vector< Gfx_RenderObject > myRenderObjects;
    Vk_FrameDrawPrep                myDrawPrep;
    bool                            myMaterialBindLoggedOnce = false;
    bool                            myBindlessLoggedOnce     = false;
    bool                            myM1PerfLoggedOnce       = false;
    Vk_BindlessCapabilities         myBindlessCaps{};
    Vk_RenderMaterialPath           myMaterialPath = Vk_RenderMaterialPath::Batch;
    UtilScenePanel::State           myScenePanelState;
    MultiViewState                  myMultiViewState;
    UtilRenderDebugPanel::State     myRenderDebugState;  // skip opaque/transparent + debug view (epic §B)
    std::vector< VkDescriptorSet >  myMaterialDescriptorSets;
#pragma endregion

private:
    WorldState*                       myWorld = nullptr;
    Vk_DeletionQueue                  myDeletionQueue;
    Vk_DeletionQueue                  mySceneDeletionQueue;
    Vk_DeletionQueue                  mySwapChainDeletionQueue;
    uint32_t                          myWidth, myHeight;
    GLFWwindow*                       myWindow;
    VkInstance                        myInstance;
    VkPhysicalDevice                  myPhysicalDevice;
    VkDevice                          myDevice;
    VkQueue                           myGraphicsQueue;
    VkQueue                           myPresentQueue;
    VkQueue                           myTransferQueue;
    VkSurfaceKHR                      mySurface;
    VkSwapchainKHR                    mySwapChain;
    VkFormat                          mySwapChainImageFormat;
    VkExtent2D                        mySwapChainExtent;
    VkDescriptorSetLayout             myGlobalSetLayout;
    VkDescriptorSetLayout             myMaterialSetLayout;
    VkDescriptorSetLayout             myBindlessMaterialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout             myObjectSetLayout;
    VkDescriptorPool                  myDescriptorPool;
    VkPipelineLayout                  myPipelineLayout;
    VkPipelineLayout                  myBindlessPipelineLayout = VK_NULL_HANDLE;
    VkRenderPass                      myRenderPass;
    VkPipeline                        myBasicPipeline;
    VkPipeline                        myTransparentPipeline;
    VkPipeline                        myBasicPipelineBindless       = VK_NULL_HANDLE;
    VkPipeline                        myTransparentPipelineBindless = VK_NULL_HANDLE;
    VkPipelineCache                   myPipelineCache               = VK_NULL_HANDLE;  // device lifetime; see Vk_DevicePipelineCache
    VkDescriptorSet                   myBindlessDescriptorSet       = VK_NULL_HANDLE;
    Vk_AllocatedBuffer                myMaterialTableBuffer;
    std::vector< Vk_AllocatedBuffer > myMaterialParamBuffers;
    VkCommandPool                     myGraphicsCommandPool;
    VkCommandPool                     myTransferCommandPool;
    Gfx_Texture                       myDepthTexture;
    Gfx_Texture                       myColorTexture;
    VkSampler                         myTextureSampler;
    uint32_t                          myTextureImageMipLevels;
    Vk_QueueFamilyIndices             myQueueFamilyIndices;

    mutable VkPhysicalDeviceProperties myPhysicalDeviceProperties;
    mutable VkPhysicalDeviceFeatures   myPhysicalDeviceFeatures;

    std::vector< Vk_FrameData >  myFrameDatas;
    std::vector< const char* >   myValidationLayers;
    std::vector< const char* >   myDeviceExtensions;
    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;
    // std::array< Vk_AllocatedBuffer, eVk_BindingCount > myUniformBuffers;  // TODO: this is planned to be an array of constant buffer

    bool                                           myEnableValidationLayers;
    bool                                           myFramebufferResized = false;
    uint32_t                                       myCurrentFrame       = 0;
    VkSampleCountFlagBits                          myMSAASamples        = VK_SAMPLE_COUNT_1_BIT;
    Util_ImGuiLayer                                myImGuiLayer;
    std::chrono::high_resolution_clock::time_point myLastFrameTime;
    bool                                           myHasLastFrameTime = false;
    std::chrono::high_resolution_clock::time_point myFrameInputSampleTime;
    bool                                           myHasFrameInputSampleTime = false;
    Vk_RenderDoc                                   myRenderDoc;
};
#pragma once
#include <array>
#include <chrono>
#include <string>
#include <vk_mem_alloc.h>

#include "../Util/Util_ImGuiLayer.h"
#include "../Util/Util_FrameStats.h"
#include "../Util/Util_InputSnapshot.h"
#include "Vk_Camera.h"
#include "Vk_DataStruct.h"
#include "Vk_Enum.h"
#include "Vk_FrameData.h"
#include "../Gfx/Gfx_DrawBatch.h"
#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_Lod.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "Vk_Bindless.h"
#include "Vk_ResourceTables.h"

constexpr int  MAX_FRAMES_IN_FLIGHT = 2;   // swapchain frames in flight; also env UBO slice count
constexpr bool USE_MANUAL_VERTICES = false;  // if true, skip OBJ load path (legacy)
constexpr bool FILL_MODE_LINE       = false;  // debug wireframe via polygon mode

struct Vk_AllocatedImage;
struct Vk_AllocatedBuffer;
struct Gfx_Texture;
struct Gfx_Material;
class Gfx_Vertex;
class Gfx_Mesh;
class Gfx_RenderObject;
struct GLFWwindow;

// Vulkan backend singleton: window, device, swapchain, frame loop, GPU resource helpers.
class Vk_ResourceTables;

class Vk_Core {
    friend class Vk_ResourceTables;  // Load path uses myAllocator/myDevice; TODO: Vk_ResourceContext instead
public:
    static Vk_Core& GetInstance();
    Vk_Core( const Vk_Core& ) = delete;
    Vk_Core& operator=( const Vk_Core& ) = delete;

    void SetSize( const uint32_t aWidth, const uint32_t aHeight );
    void SetVsync( bool aVsync );
    void Reset();

    // Application lifecycle (orchestrated by App/Application).
    void InitWindow();
    void InitRenderDevice();
    void LoadSceneResources( Gfx_SceneDesc aScene );
    void UnloadScene();
    void Shutdown();
    void BeginPlatformFrame( float& aOutDeltaSeconds );
    void ApplyCameraInput( float aDeltaSeconds, const Util_InputSnapshot& aInput );
    void SetFrameInputSampleTime( std::chrono::high_resolution_clock::time_point aSampleTime );
    void Render();
    bool ShouldClose() const;
    GLFWwindow* GetWindow() const { return myWindow; }

    // TODO: maybe merge all the set function when initializing?
    void SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers );
    void SetRequiredExtension( std::vector< const char* > someDeviceExtensions );

    // Resource helpers (used by Gfx/Util loaders; prefer injecting context long-term).
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

    // Init Functions:
    // Part 1: Base
    void CreateInstance();
    void PickPhysicalDevice();
    void InitVk_QueueFamilyIndices();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateCommandPool();
    void InitAllocator();

    // Part 2: Swap chain
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateBindlessMaterialSetLayout();
    void CreateBindlessPipelineLayout();
    void CreateBindlessGfxPipelines();
    void CreateBindlessDescriptorResources();
    void CreateGfxPipeline();
    void CreateDepthResources();
    void CreateColorResources();
    void CreateFrameBuffers();

    // Part 3: Resources
    void CreateTextureSampler();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateMaterialDescriptorSets();

    // Part 4: Prepare for draw frames
    void CreateFrameData();
    void CreateInstanceSlabs();
    void CreateCamera();
    void InitDefaultEnvironmentData();
    void CreateUniformBuffers();
    void InitImGui();
    void ShutdownImGui();

    // Draw frame:
    void DrawFrame( const Vk_FrameData aFrameData );
    void RecreateSwapChain();
    void RefreshMaterialPipelinesAfterSwapchainRecreate();
    void LogM1PerfSnapshot() const;
    void UpdateUniformBuffer( uint32_t aCurrentFrame ) const;
    bool FillInstanceSlab( uint32_t aCurrentFrame );
    size_t InstanceSlabStride() const;
    void ApplyDemoTransformAnimation();
    // Live Vulkan scene path: Gfx_FrameExtract → cull/sort/batch → RecordScenePass.
    void RecordScenePass( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    void RecordDrawBatches( VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, const std::vector< Gfx_BatchRun >& aBatchRuns );
    void RecordDrawBatchesBindless( VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, VkPipeline aPipeline );
    void RecordImGuiPass( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    // Required when pipeline uses VK_DYNAMIC_STATE_VIEWPORT / LINE_WIDTH (SetDefaultDynamicStates).
    void SetGraphicsDynamicState( VkCommandBuffer aCommandBuffer ) const;
    glm::mat4 ComputeDemoModelMatrix( const glm::mat4& aWorldTransform ) const;

    // Helper functions:
    void                    CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void                    CheckExtensionSupport() const;
    bool                    CheckValidationLayerSupport() const;
    bool                    CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const;
    bool                    CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const;
    Vk_QueueFamilyIndices      FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const;
    Vk_SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const;
    VkSurfaceFormatKHR      ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats ) const;
    VkPresentModeKHR        ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes ) const;
    VkExtent2D              ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities ) const;
    uint32_t                FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties ) const;
    VkCommandBuffer         BeginSingleTimeCommands( VkCommandPool aCommandPool ) const;
    void                    EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const;
    VkFormat                FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures ) const;
    VkFormat                FindDepthFormat() const;
    VkSampleCountFlagBits   GetMaxUsableSampleCount() const;
    size_t                  PadUniformBufferSize( size_t anOriginalSize ) const;

    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    bool         myVsync       = true;
    uint32_t     myFrameNumber = 0;
    Util_FrameStats   myFrameStats;
    VmaAllocator myAllocator;

#pragma region View Data Functions
    Vk_Camera            myCamera;
    Util_CameraSettings  myCameraSettings;
    GpuEnvironmentData   myEnvironmentData;
    Vk_AllocatedBuffer    myEnvDataBuffer;

    Vk_ResourceTables                            myResourceTables;
    std::vector< Gfx_RenderObject >              myRenderObjects;
    Gfx_SceneSoA                                 mySceneSoA;
    Gfx_LodTable                                 myLodTable;
    Gfx_LodState                                 myLodState;
    Gfx_FrameExtract                             myFrameExtract;
    std::vector< Gfx_BatchRun >                  myOpaqueBatchRuns;
    std::vector< Gfx_BatchRun >                  myTransparentBatchRuns;
    bool                                         myExtractLoggedOnce        = false;
    bool                                         myBatchLoggedOnce          = false;
    bool                                         myTransLoggedOnce          = false;
    bool                                         myInstanceSlabOverflowLogged = false;
    bool                                         myMaterialBindLoggedOnce     = false;
    bool                                         myLodLoggedOnce              = false;
    uint32_t                                     myLodDebugLogicalMeshId      = UINT32_MAX;
    bool                                         myBindlessLoggedOnce         = false;
    bool                                         myM1PerfLoggedOnce           = false;
    Vk_BindlessCapabilities                      myBindlessCaps{};
    Vk_RenderMaterialPath                        myMaterialPath               = Vk_RenderMaterialPath::Batch;
    std::vector< glm::mat4 >                     myDemoBaseTransforms;
    Gfx_SceneDesc                                myLoadedScene;
    Gfx_SceneIdTables                            mySceneIdTables;
    bool                                         myHasLoadedScene = false;
    std::vector< VkDescriptorSet >               myMaterialDescriptorSets;
#pragma endregion

private:
    Vk_DeletionQueue         myDeletionQueue;
    Vk_DeletionQueue         mySwapChainDeletionQueue;
    uint32_t              myWidth, myHeight;
    GLFWwindow*           myWindow;
    VkInstance            myInstance;
    VkPhysicalDevice      myPhysicalDevice;
    VkDevice              myDevice;
    VkQueue               myGraphicsQueue;
    VkQueue               myPresentQueue;
    VkQueue               myTransferQueue;
    VkSurfaceKHR          mySurface;
    VkSwapchainKHR        mySwapChain;
    VkFormat              mySwapChainImageFormat;
    VkExtent2D            mySwapChainExtent;
    VkDescriptorSetLayout myGlobalSetLayout;
    VkDescriptorSetLayout myMaterialSetLayout;
    VkDescriptorSetLayout myBindlessMaterialSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout myObjectSetLayout;
    VkDescriptorPool      myDescriptorPool;
    VkPipelineLayout      myPipelineLayout;
    VkPipelineLayout      myBindlessPipelineLayout = VK_NULL_HANDLE;
    VkRenderPass          myRenderPass;
    VkPipeline            myBasicPipeline;
    VkPipeline            myTransparentPipeline;
    VkPipeline            myBasicPipelineBindless       = VK_NULL_HANDLE;
    VkPipeline            myTransparentPipelineBindless = VK_NULL_HANDLE;
    VkDescriptorSet       myBindlessDescriptorSet       = VK_NULL_HANDLE;
    Vk_AllocatedBuffer    myMaterialTableBuffer;
    std::vector< Vk_AllocatedBuffer >            myMaterialParamBuffers;
    VkCommandPool         myGraphicsCommandPool;
    VkCommandPool         myTransferCommandPool;
    Gfx_Texture               myDepthTexture;
    Gfx_Texture               myColorTexture;
    VkSampler             myTextureSampler;
    uint32_t              myTextureImageMipLevels;
    Vk_QueueFamilyIndices    myQueueFamilyIndices;

    mutable VkPhysicalDeviceProperties myPhysicalDeviceProperties;
    mutable VkPhysicalDeviceFeatures   myPhysicalDeviceFeatures;

    std::vector< Vk_FrameData >     myFrameDatas;
    std::vector< const char* >   myValidationLayers;
    std::vector< const char* >   myDeviceExtensions;
    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;
    // std::array< Vk_AllocatedBuffer, eVk_BindingCount > myUniformBuffers;  // TODO: this is planned to be an array of constant buffer

    bool                  myEnableValidationLayers;
    bool                  myFramebufferResized = false;
    uint32_t              myCurrentFrame       = 0;
    VkSampleCountFlagBits myMSAASamples        = VK_SAMPLE_COUNT_1_BIT;
    Util_ImGuiLayer                                              myImGuiLayer;
    std::chrono::high_resolution_clock::time_point             myLastFrameTime;
    bool                                                       myHasLastFrameTime = false;
    std::chrono::high_resolution_clock::time_point             myFrameInputSampleTime;
    bool                                                       myHasFrameInputSampleTime = false;
};
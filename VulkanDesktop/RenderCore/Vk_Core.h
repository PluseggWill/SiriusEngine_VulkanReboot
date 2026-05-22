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

constexpr int  MAX_FRAMES_IN_FLIGHT = 2;   // swapchain frames in flight; also env UBO slice count
constexpr bool USE_RUNTIME_MIPMAP   = false;
constexpr bool USE_MANUAL_VERTICES  = false;  // if true, skip OBJ load path (legacy)
constexpr bool ENABLE_ROTATE        = true;   // demo: spin model in GpuCameraData.model
constexpr bool FILL_MODE_LINE       = false;  // debug wireframe via polygon mode

struct Vk_AllocatedImage;
struct Vk_AllocatedBuffer;
struct Gfx_Texture;
struct Gfx_Material;
class Gfx_Vertex;
class Gfx_Mesh;
class Gfx_RenderObject;

// Vulkan backend singleton: window, device, swapchain, frame loop, GPU resource helpers.
// Scene tables (myMeshMap, etc.) live here temporarily - see EngineArchitecture section 3.1 for split target.
class Vk_Core {
public:
    static Vk_Core& GetInstance();
    Vk_Core( const Vk_Core& ) = delete;
    Vk_Core& operator=( const Vk_Core& ) = delete;

    void SetSize( const uint32_t aWidth, const uint32_t aHeight );
    void Run();
    void Reset();
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

    void InitWindow();
    void MainLoop();
    void Clear();
    void InitVulkan();

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
    void CreateGfxPipeline();
    void CreateDepthResources();
    void CreateColorResources();
    void CreateFrameBuffers();

    // Part 3: Resources
    void CreateTextureSampler();
    void CreateDescriptorPool();
    void CreateDescriptorSets();

    // Part 4: Prepare for draw frames
    void CreateFrameData();
    void CreateCamera();
    void InitDefaultEnvironmentData();
    void CreateUniformBuffers();
    void InitScene();
    void InitImGui();
    void ShutdownImGui();
    // Order: poll events -> dt -> ImGui NewFrame -> input sample -> camera. Call once before DrawFrame.
    void BeginFrame( float& aOutDeltaSeconds );

    // Draw frame:
    void DrawFrame( const Vk_FrameData aFrameData );
    void RecreateSwapChain();
    void UpdateUniformBuffer( uint32_t aCurrentFrame ) const;
    void RecordScenePass( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    void RecordImGuiPass( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    void DrawObjects( VkCommandBuffer aCommandBuffer, std::vector< Gfx_RenderObject >& someRenderObjects, uint32_t anImageIndex );

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

#pragma region View Data Functions
    Gfx_Material* CreateMaterial( VkPipeline aPipeline, VkPipelineLayout aLayout, const uint32_t index );
    Gfx_Material* GetMaterial( const uint32_t anIndex );
    Gfx_Mesh*     CreateMesh( const std::string& aFilename, const uint32_t anIndex );
    Gfx_Mesh*     GetMesh( const uint32_t anIndex );
    Gfx_Texture*  CreateTexture( const std::string& aFilename, const uint32_t anIndex );
    Gfx_Texture*  GetTexture( const uint32_t anIndex );
#pragma endregion

    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    uint32_t     myFrameNumber = 0;
    Util_FrameStats   myFrameStats;
    VmaAllocator myAllocator;

#pragma region View Data Functions
    Vk_Camera            myCamera;
    Util_CameraSettings  myCameraSettings;
    Util_InputState      myInputState;
    GpuEnvironmentData   myEnvironmentData;
    Vk_AllocatedBuffer    myEnvDataBuffer;

    std::unordered_map< uint32_t, Gfx_Material > myMaterialMap;
    std::unordered_map< uint32_t, Gfx_Mesh >     myMeshMap;
    std::unordered_map< uint32_t, Gfx_Texture >  myTextureMap;
    std::vector< Gfx_RenderObject >              myRenderObjects;
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
    VkDescriptorPool      myDescriptorPool;
    VkPipelineLayout      myPipelineLayout;
    VkRenderPass          myRenderPass;
    VkPipeline            myBasicPipeline;
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
};
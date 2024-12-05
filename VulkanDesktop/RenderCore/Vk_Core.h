#pragma once
#include <array>
#include <string>
#include <vk_mem_alloc.h>

#include "Vk_Camera.h"
#include "Vk_DataStruct.h"
#include "Vk_Enum.h"
#include "Vk_FrameData.h"

constexpr int   MAX_FRAMES_IN_FLIGHT = 2;
constexpr bool  USE_RUNTIME_MIPMAP   = false;
constexpr bool  USE_MANUAL_VERTICES  = false;
constexpr bool  ENABLE_ROTATE        = true;
constexpr bool  FILL_MODE_LINE       = false;
constexpr float SPEED                = 0.3f;
// const bool ENABLE_MSAA          = false;

struct AllocatedImage;
struct AllocatedBuffer;
struct Texture;
struct Material;
class Vertex;
class Mesh;
class RenderObject;

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

    // Util Functions:
    void           CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, AllocatedBuffer& aBuffer, bool isExclusive ) const;
    void           CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage,
                                AllocatedImage& anImage ) const;
    void           CreateImage( VkExtent2D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, AllocatedImage& anImage ) const;
    void           CreateImage( VkExtent3D anExtent, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags anImageUsage, VmaMemoryUsage aMemoryUsage, uint32_t aMipLevel,
                                VkSampleCountFlagBits aNumSamples, AllocatedImage& anImage ) const;
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
    void InitQueueFamilyIndices();
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
    void CreateUniformBuffers();
    void InitScene();

    // Draw frame:
    void DrawFrame( const FrameData aFrameData );
    void RecreateSwapChain();
    void UpdateUniformBuffer( uint32_t aCurrentFrame ) const;
    void RecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    void DrawObjects( VkCommandBuffer aCommandBuffer, std::vector< RenderObject >& someRenderObjects, uint32_t anImageIndex );

    // Helper functions:
    void                    CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void                    CheckExtensionSupport() const;
    bool                    CheckValidationLayerSupport() const;
    bool                    CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice ) const;
    bool                    CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const;
    QueueFamilyIndices      FindQueueFamilies( VkPhysicalDevice aPhysicalDevice ) const;
    SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice ) const;
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
    Material* CreateMaterial( VkPipeline aPipeline, VkPipelineLayout aLayout, const uint32_t index );
    Material* GetMaterial( const uint32_t anIndex );
    Mesh*     CreateMesh( const std::string& aFilename, const uint32_t anIndex );
    Mesh*     GetMesh( const uint32_t anIndex );
    Texture*  CreateTexture( const std::string& aFilename, const uint32_t anIndex );
    Texture*  GetTexture( const uint32_t anIndex );
#pragma endregion

    // GLFW callback functions: GLFW does not know how to properly call a member funtion with the right "this" pointer.
    static void HandleInputCallback( GLFWwindow* aWindow, int aKey, int aScanCode, int anAction, int aMode );
    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    uint32_t     myFrameNumber = 0;
    VmaAllocator myAllocator;

#pragma region View Data Functions
    Gfx_Camera         myCamera;
    GpuEnvironmentData myEnvironmentData;
    AllocatedBuffer    myEnvDataBuffer;

    std::unordered_map< uint32_t, Material > myMaterialMap;
    std::unordered_map< uint32_t, Mesh >     myMeshMap;
    std::unordered_map< uint32_t, Texture >  myTextureMap;
    std::vector< RenderObject >              myRenderObjects;
#pragma endregion

private:
    DeletionQueue         myDeletionQueue;
    DeletionQueue         mySwapChainDeletionQueue;
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
    Texture               myDepthTexture;
    Texture               myColorTexture;
    VkSampler             myTextureSampler;
    uint32_t              myTextureImageMipLevels;
    QueueFamilyIndices    myQueueFamilyIndices;

    mutable VkPhysicalDeviceProperties myPhysicalDeviceProperties;
    mutable VkPhysicalDeviceFeatures   myPhysicalDeviceFeatures;

    std::vector< FrameData >     myFrameDatas;
    std::vector< const char* >   myValidationLayers;
    std::vector< const char* >   myDeviceExtensions;
    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;
    // std::array< AllocatedBuffer, eVk_BindingCount > myUniformBuffers;  // TODO: this is planned to be an array of constant buffer

    bool                  myEnableValidationLayers;
    bool                  myFramebufferResized = false;
    uint32_t              myCurrentFrame       = 0;
    VkSampleCountFlagBits myMSAASamples        = VK_SAMPLE_COUNT_1_BIT;
};
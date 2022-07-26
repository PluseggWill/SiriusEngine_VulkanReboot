#pragma once

#include "Vk_DataStruct.h"
#include "Vk_Mesh.h"
#include "Vk_Camera.h"
#include "Vk_RenderObject.h"

// #include <unordered_map>

const int  MAX_FRAMES_IN_FLIGHT = 2;
const bool USE_RUNTIME_MIPMAP   = false;
const bool USE_MANUAL_VERTICES  = false;
const bool ENABLE_ROTATE        = true;
const bool FILL_MODE_LINE       = false;
const float SPEED                = 0.3f;
// const bool ENABLE_MSAA          = false;

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

    void TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel ) const;
    void CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight ) const;
    void GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel ) const;
    void CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    bool HasStencilComponent( VkFormat aFormat ) const;

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
    void InitQueueFamilyIndice();
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
    void CreateTexture();
    void CreateTextureSampler();
    void FillVerticesData();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();

    // Part 4: Prepare for draw frames
    void CreateGraphicsCommandBuffers();
    void CreateSyncObjects();
    void CreateCamera();

    // Draw frame:
    void DrawFrame();
    void RecreateSwapChain();
    void UpdateUniformBuffer( uint32_t anImageIndex );
    void RecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    void DrawObjects( VkCommandBuffer aCommandBuffer, std::vector< RenderObject >& someRenderObjects, uint32_t anImageIndex );

    // Helper functions:
    void                    CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize ) const;
    void                    CheckExtensionSupport() const;
    bool                    CheckValidationLayerSupport() const;
    bool                    CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice );
    bool                    CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice ) const;
    QueueFamilyIndices      FindQueueFamilies( VkPhysicalDevice aPhysicalDevice );
    SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice );
    VkSurfaceFormatKHR      ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats );
    VkPresentModeKHR        ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes );
    VkExtent2D              ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities );
    uint32_t                FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties );
    VkCommandBuffer         BeginSingleTimeCommands( VkCommandPool aCommandPool ) const;
    void                    EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const;
    VkFormat                FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures );
    VkFormat                FindDepthFormat();
    VkSampleCountFlagBits   GetMaxUsableSampleCount() const;
    Material*               CreateMaterial( VkPipeline aPipeline, VkPipelineLayout aLayout, const uint32_t index );
    Material*               GetMaterial( const uint32_t index );
    //Texture*                CreateTexture();
    Texture*                GetTexutre();
    Mesh*                   CreateMesh( const std::string& aFilename, const uint32_t anIndex );
    Mesh*                   GetMesh( const uint32_t index );

    // GLFW callback functions: GLFW does not know how to properly call a member funtion with the right "this" pointer.
    static void HandleInputCallback( GLFWwindow* aWindow, int aKey, int aScanCode, int anAction, int aMode );
    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    int          myFrameNumber = 0;
    VmaAllocator myAllocator;
    Camera       myCamera;

    std::unordered_map< uint32_t, Material > myMaterialMap;
    std::unordered_map< uint32_t, Mesh >     myMeshMap;
    std::unordered_map< uint32_t, Texture >  myTextureMap;
    std::vector< RenderObject >              myRenderObjects;

private:
    DeletionQueue         myDeletionQueue;
    DeletionQueue         mySwapChainDeletionQueue;
    uint32_t              myWidth, myHeight;
    GLFWwindow*           myWindow;
    VkInstance            myInstance;
    VkPhysicalDevice      myPhysicalDevice = VK_NULL_HANDLE;
    VkDevice              myDevice;
    VkQueue               myGraphicsQueue;
    VkQueue               myPresentQueue;
    VkQueue               myTransferQueue;
    VkSurfaceKHR          mySurface;
    VkSwapchainKHR        mySwapChain;
    VkFormat              mySwapChainImageFormat;
    VkExtent2D            mySwapChainExtent;
    VkDescriptorSetLayout myDescriptorSetLayout;
    VkPipelineLayout      myPipelineLayout;
    VkRenderPass          myRenderPass;
    VkPipeline            myBasicPipeline;
    VkCommandPool         myGraphicsCommandPool;
    VkCommandPool         myTransferCommandPool;
    VkDescriptorPool      myDescriptorPool;
    uint32_t              myTextureImageMipLevels;
    Texture               myTexture;
    Texture               myDepthTexture;
    Texture               myColorTexture;
    VkSampler             myTextureSampler;
    QueueFamilyIndices    myQueueFamilyIndices;

    std::vector< VkDescriptorSet > myDescriptorSets;
    std::vector< AllocatedBuffer > myUniformBuffers;
    std::vector< VkCommandBuffer > myGraphicsCommandBuffers;
    std::vector< VkSemaphore >     myImageAvailableSemaphores;
    std::vector< VkSemaphore >     myRenderFinishedSemaphores;
    std::vector< VkFence >         myInFlightFences;
    std::vector< const char* >     myValidationLayers;
    std::vector< const char* >     myDeviceExtensions;
    std::vector< VkImage >         mySwapChainImages;
    std::vector< VkImageView >     mySwapChainImageViews;
    std::vector< VkFramebuffer >   mySwapChainFrameBuffers;

    bool                  myEnableValidationLayers;
    bool                  myFramebufferResized = false;
    uint32_t              myCurrentFrame       = 0;
    VkSampleCountFlagBits myMSAASamples        = VK_SAMPLE_COUNT_1_BIT;

    //Mesh myMesh;
};
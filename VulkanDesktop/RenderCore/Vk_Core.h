#pragma once

#include "Vk_DataStruct.h"
#include "Vk_Mesh.h"

const int  MAX_FRAMES_IN_FLIGHT = 2;
const bool USE_RUNTIME_MIPMAP   = false;
const bool USE_MANUAL_VERTICES  = false;
const bool ENABLE_ROTATE        = false;
const bool FILL_MODE_LINE       = true;
// const bool ENABLE_MSAA          = false;

class Vk_Core {
public:
    static Vk_Core& GetInstance();
    // static Vk_Core* GetInstance();
    void SetSize( const uint32_t aWidth, const uint32_t aHeight );
    void Run();
    void Reset();
    // TODO: maybe merge all the set function when initializing?
    void SetEnableValidationLayers( bool aEnableValidationLayers, std::vector< const char* > someValidationLayers );
    void SetRequiredExtension( std::vector< const char* > someDeviceExtensions );

private:
    Vk_Core();
    ~Vk_Core();
    Vk_Core( const Vk_Core& ) = delete;
    Vk_Core& operator=( const Vk_Core& ) = delete;

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
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();
    void FillVerticesData();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();

    // Part 4: Prepare for draw frames
    void CreateGraphicsCommandBuffers();
    void CreateSyncObjects();

    // Draw frame:
    void DrawFrame();
    void RecreateSwapChain();
    void UpdateUniformBuffer( uint32_t aCurrentImage );
    void RecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );

    // Helper functions:
    VkShaderModule          CreateShaderModule( const std::vector< char >& someShaderCode );
    VkShaderModule          CreateShaderModule( const std::string aShaderPath );
    void                    CreateImage( uint32_t aWidth, uint32_t aHeight, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkImage& aImage,
                                         VkDeviceMemory& aImageMemory, uint32_t aMipLevel = 1, VkSampleCountFlagBits aNumSamples = VK_SAMPLE_COUNT_1_BIT );
    VkImageView             CreateImageView( VkImage aImage, VkFormat aFormat, VkImageAspectFlags someAspectFlags, uint32_t aMipLevel = 1 );
    void                    TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel );
    void                    CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight );
    void                    GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel );
    void                    CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkBuffer& aBuffer, VkDeviceMemory& aBufferMemory, bool isExclusive );
    void                    CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aBufferUsage, VmaMemoryUsage aMemoryUsage, AllocatedBuffer& aAllocatedBuffer, bool isExclusive );
    void                    CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    CheckExtensionSupport();
    bool                    CheckValidationLayerSupport();
    bool                    CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice );
    bool                    CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice );
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
    bool                    HasStencilComponent( VkFormat aFormat );
    VkSampleCountFlagBits   GetMaxUsableSampleCount() const;

    // GLFW callback functions: GLFW does not know how to properly call a member funtion with the right "this" pointer.
    static void HandleInputCallback( GLFWwindow* aWindow, int aKey, int aScanCode, int anAction, int aMode );
    static void FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );

public:
    int myFrameNumber = 0;

private:
    DeletionQueue myDeletionQueue;
    DeletionQueue mySwapChainDeletionQueue;

    uint32_t myWidth, myHeight;

    GLFWwindow* myWindow;

    VmaAllocator myAllocator;

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
    /*VkBuffer              myVertexBuffer;
    VkDeviceMemory        myVertexBufferMemory;
    VkBuffer              myIndexBuffer;
    VkDeviceMemory        myIndexBufferMemory;*/
    VkDescriptorPool   myDescriptorPool;
    uint32_t           myTextureImageMipLevels;
    VkImage            myTextureImage;
    VkDeviceMemory     myTextureImagememory;
    VkImageView        myTextureImageView;
    VkSampler          myTextureSampler;
    VkDeviceMemory     myTextureImageMemory;
    VkImage            myDepthImage;
    VkDeviceMemory     myDepthImageMemory;
    VkImageView        myDepthImageView;
    VkImage            myColorImage;
    VkDeviceMemory     myColorImageMemory;
    VkImageView        myColorImageView;
    QueueFamilyIndices myQueueFamilyIndices;

    std::vector< VkDescriptorSet > myDescriptorSets;
    std::vector< VkBuffer >        myUniformBuffers;
    std::vector< VkDeviceMemory >  myUniformBuffersMemory;
    std::vector< VkCommandBuffer > myGraphicsCommandBuffers;
    std::vector< VkSemaphore >     myImageAvailableSemaphores;
    std::vector< VkSemaphore >     myRenderFinishedSemaphores;
    std::vector< VkFence >         myInFlightFences;

    std::vector< const char* >   myValidationLayers;
    std::vector< const char* >   myDeviceExtensions;
    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;

    bool                  myEnableValidationLayers;
    bool                  myFramebufferResized = false;
    uint32_t              myCurrentFrame       = 0;
    VkSampleCountFlagBits myMSAASamples        = VK_SAMPLE_COUNT_1_BIT;

    Mesh myMesh;
};
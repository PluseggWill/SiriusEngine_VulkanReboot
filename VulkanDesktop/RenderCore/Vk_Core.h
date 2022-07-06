#pragma once

#include "Gfx_DataStruct.h"

const int  MAX_FRAMES_IN_FLIGHT = 2;
const bool USE_RUNTIME_MIPMAP   = false;
const bool USE_MANUAL_VERTICES  = false;
const bool ENABLE_ROTATE        = false;
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

    void InitDevice();
    void CreateInstance();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();

    void InitSwapChian();
    void CreateSwapChain();
    void CreateImageViews();

    void InitResources();

    void CreateGfxPipeline();
    void CreateRenderPass();
    void CreateFrameBuffers();
    void CreateCommandPool();
    void CreateGraphicsCommandBuffers();
    void CreateSyncObjects();
    void DrawFrame();
    void FillVerticesData();
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void RecreateSwapChain();
    void CleanupSwapChain();
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();
    void CreateDepthResources();
    void CreateColorResources();
    void InitQueueFamilyIndice();

    void                    UpdateUniformBuffer( uint32_t aCurrentImage );
    VkImageView             CreateImageView( VkImage aImage, VkFormat aFormat, VkImageAspectFlags someAspectFlags, uint32_t aMipLevel = 1 );
    void                    CreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkBuffer& aBuffer, VkDeviceMemory& aBufferMemory, bool isExclusive );
    void                    CopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    CopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    CheckExtensionSupport();
    void                    RecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    bool                    CheckValidationLayerSupport();
    bool                    CheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice );
    bool                    CheckExtensionSupport( VkPhysicalDevice aPhysicalDevice );
    QueueFamilyIndices      FindQueueFamilies( VkPhysicalDevice aPhysicalDevice );
    SwapChainSupportDetails QuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice );
    VkSurfaceFormatKHR      ChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats );
    VkPresentModeKHR        ChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes );
    VkExtent2D              ChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities );
    VkShaderModule          CreateShaderModule( const std::vector< char >& someShaderCode );
    static void             FramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );
    uint32_t                FindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties );
    VkCommandBuffer         BeginSingleTimeCommands( VkCommandPool aCommandPool ) const;
    void                    EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue ) const;
    void                    TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout, uint32_t aMipLevel );
    void                    CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight );
    VkFormat                FindSupportedFormat( const std::vector< VkFormat >& someCandidates, VkImageTiling aTiling, VkFormatFeatureFlagBits someFeatures );
    VkFormat                FindDepthFormat();
    bool                    HasStencilComponent( VkFormat aFormat );
    void                    GenerateMipmaps( VkImage aImage, VkFormat aImageFormat, int32_t aTexWidth, int32_t aTexHeight, uint32_t aMipLevel );
    VkSampleCountFlagBits   GetMaxUsableSampleCount() const;
    void                    CreateImage( uint32_t aWidth, uint32_t aHeight, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkImage& aImage,
                                         VkDeviceMemory& aImageMemory, uint32_t aMipLevel = 1, VkSampleCountFlagBits aNumSamples = VK_SAMPLE_COUNT_1_BIT );

public:
private:
    uint32_t myWidth, myHeight;

    GLFWwindow* myWindow;

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
    VkPipeline            myGfxPipeline;
    VkCommandPool         myGraphicsCommandPool;
    VkCommandPool         myTransferCommandPool;
    VkBuffer              myVertexBuffer;
    VkDeviceMemory        myVertexBufferMemory;
    VkBuffer              myIndexBuffer;
    VkDeviceMemory        myIndexBufferMemory;
    VkDescriptorPool      myDescriptorPool;
    uint32_t              myTextureImageMipLevels;
    VkImage               myTextureImage;
    VkDeviceMemory        myTextureImagememory;
    VkImageView           myTextureImageView;
    VkSampler             myTextureSampler;
    VkDeviceMemory        myTextureImageMemory;
    VkImage               myDepthImage;
    VkDeviceMemory        myDepthImageMemory;
    VkImageView           myDepthImageView;
    VkImage               myColorImage;
    VkDeviceMemory        myColorImageMemory;
    VkImageView           myColorImageView;
    QueueFamilyIndices    myQueueFamilyIndices;

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

    std::vector< Vertex >                  vertices;
    std::vector< uint32_t >                indices;
    std::unordered_map< Vertex, uint32_t > uniqueVertices{};
};
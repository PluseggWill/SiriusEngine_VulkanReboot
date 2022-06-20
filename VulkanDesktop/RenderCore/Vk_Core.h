#pragma once
#include "../Util/Util_Include.h"
#include "../Util/Util_Loader.h"
#include "Gfx_DataStruct.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

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
    void CreateInstance();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    void CreateSwapChain();
    void CreateImageViews();
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

    void                    UpdateUniformBuffer( uint32_t aCurrentImage );
    void                    CreateImage( uint32_t aWidth, uint32_t aHeight, VkFormat aFormat, VkImageTiling aTiling, VkImageUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkImage& aImage,
                                         VkDeviceMemory& aImageMemory );
    VkImageView             CreateImageView( VkImage aImage, VkFormat aFormat );
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
    VkCommandBuffer         BeginSingleTimeCommands( VkCommandPool aCommandPool );
    void                    EndSingleTimeCommands( VkCommandBuffer aCommandBuffer, VkCommandPool aCommandPool, VkQueue aQueue );
    void                    TransitionImageLayout( VkImage aImage, VkFormat aFormat, VkImageLayout anOldLayout, VkImageLayout aNewLayout );
    void                    CopyBufferToImage( VkBuffer aBuffer, VkImage aImage, uint32_t aWidth, uint32_t aHeight );
    // VkResult myVkCreateInstance(const VkInstanceCreateInfo* aCreateInfo, const VkAllocationCallbacks* aAllocator, VkInstance* aInstance);

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
    VkImage               myTextureImage;
    VkDeviceMemory        myTextureImagememory;
    VkImageView           myTextureImageView;
    VkSampler             myTextureSampler;
    VkDeviceMemory        myTextureImageMemory;
    VkImage               myDepthImage;
    VkDeviceMemory        myDepthImageMemroy;
    VkImageView           myDepthImageView;

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

    bool     myEnableValidationLayers;
    bool     myFramebufferResized = false;
    uint32_t myCurrentFrame       = 0;

    std::vector< Vertex >   vertices;
    std::vector< uint16_t > indices;
};
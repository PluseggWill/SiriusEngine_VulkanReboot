#pragma once
#include "../Util/Util_Include.h"
#include "../Util/Util_Loader.h"
#include "Gfx_DataStruct.h"

//#define MAX_FRAMES_IN_FLIGHT = 2;
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

    void myInitWindow();
    void myMainLoop();
    void myClear();

    void myInitVulkan();
    void myCreateInstance();
    void myPickPhysicalDevice();
    void myCreateLogicalDevice();
    void myCreateSurface();
    void myCreateSwapChain();
    void myCreateImageViews();
    void myCreateGfxPipeline();
    void myCreateRenderPass();
    void myCreateFrameBuffers();
    void myCreateCommandPool();
    void myCreateGraphicsCommandBuffers();
    void myCreateSyncObjects();
    void myDrawFrame();
    void myFillVerticesData();
    void myCreateVertexBuffer();
    void myCreateIndexBuffer();
    void myRecreateSwapChain();
    void myCleanupSwapChain();
    void myCreateDescriptorSetLayout();
    void myCreateUniformBuffers();
    void myCreateDescriptorPool();
    void myCreateDescriptorSets();

    void                    myUpdateUniformBuffer(uint32_t aCurrentImage);
    void                    myCreateBuffer( VkDeviceSize aSize, VkBufferUsageFlags aUsage, VkMemoryPropertyFlags someProperties, VkBuffer& aBuffer, VkDeviceMemory& aBufferMemory, bool isExclusive );
    void                    myCopyBuffer( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    myCopyBufferGraphicsQueue( VkBuffer aSrcBuffer, VkBuffer aDstBuffer, VkDeviceSize aSize );
    void                    myCheckExtensionSupport();
    void                    myRecordCommandBuffer( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    bool                    myCheckValidationLayerSupport();
    bool                    myCheckDeviceSuitable( VkPhysicalDevice aPhysicalDevice );
    bool                    myCheckExtensionSupport( VkPhysicalDevice aPhysicalDevice );
    QueueFamilyIndices      myFindQueueFamilies( VkPhysicalDevice aPhysicalDevice );
    SwapChainSupportDetails myQuerySwapChainSupport( VkPhysicalDevice aPhysicalDevice );
    VkSurfaceFormatKHR      myChooseSwapSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& someAvailableFormats );
    VkPresentModeKHR        myChooseSwapPresentMode( const std::vector< VkPresentModeKHR >& someAvailablePresentModes );
    VkExtent2D              myChooseSwapExtent( const VkSurfaceCapabilitiesKHR& aCapabilities );
    VkShaderModule          myCreateShaderModule( const std::vector< char >& someShaderCode );
    static void             myFramebufferResizeCallback( GLFWwindow* aWindow, int aWidth, int aHeight );
    uint32_t                myFindMemoryType( uint32_t aTypeFiler, VkMemoryPropertyFlags someProperties );

    // VkResult myVkCreateInstance(const VkInstanceCreateInfo* aCreateInfo, const VkAllocationCallbacks* aAllocator, VkInstance* aInstance);

public:
private:
    uint32_t myWidth, myHeight;

    GLFWwindow* myWindow;

    VkInstance       myInstance;
    VkPhysicalDevice myPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         myDevice;
    VkQueue          myGraphicsQueue;
    VkQueue          myPresentQueue;
    VkQueue          myTransferQueue;
    VkSurfaceKHR     mySurface;
    VkSwapchainKHR   mySwapChain;
    VkFormat         mySwapChainImageFormat;
    VkExtent2D       mySwapChainExtent;
    VkDescriptorSetLayout myDescriptorSetLayout;
    VkPipelineLayout myPipelineLayout;
    VkRenderPass     myRenderPass;
    VkPipeline       myGfxPipeline;
    VkCommandPool    myGraphicsCommandPool;
    VkCommandPool  myTransferCommandPool;
    VkBuffer       myVertexBuffer;
    VkDeviceMemory myVertexBufferMemory;
    VkBuffer       myIndexBuffer;
    VkDeviceMemory myIndexBufferMemory;
    VkDescriptorPool      myDescriptorPool;

    std::vector< VkBuffer>      myUniformBuffers;
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
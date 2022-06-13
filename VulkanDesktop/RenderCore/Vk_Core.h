#pragma once
#define GLFW_INCLUDE_VULKAN
//#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
//#define GLFW_EXPOSE_NATIVE_WIN32
//#include <GLFW/glfw3native.h>
#include "../Util/Util_Loader.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

//#define MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_FRAMES_IN_FLIGHT = 2;

struct QueueFamilyIndices {
    std::optional< uint32_t > myGraphicsFamily;
    std::optional< uint32_t > myPresentFamily;

    bool isComplete() {
        return myGraphicsFamily.has_value() && myPresentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          myCapabilities;
    std::vector< VkSurfaceFormatKHR > myFormats;
    std::vector< VkPresentModeKHR >   myPresentModes;
};

class Vk_Core {
public:
    static Vk_Core& GetInstance();
    // static Vk_Core* GetInstance();
    void SetSize( uint32_t aWidth, uint32_t aHeight );
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
    void myCreateCommandBuffers();
    void myCreateSyncObjects();
    void myDrawFrame();
    void myRecreateSwapChain();
    void myCleanupSwapChain();

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
    VkSurfaceKHR     mySurface;
    VkSwapchainKHR   mySwapChain;
    VkFormat         mySwapChainImageFormat;
    VkExtent2D       mySwapChainExtent;
    VkPipelineLayout myPipelineLayout;
    VkRenderPass     myRenderPass;
    VkPipeline       myGfxPipeline;
    VkCommandPool    myCommandPool;

    std::vector< VkCommandBuffer > myCommandBuffers;
    std::vector< VkSemaphore >     myImageAvailableSemaphores;
    std::vector< VkSemaphore >     myRenderFinishedSemaphores;
    std::vector< VkFence >         myInFlightFences;

    std::vector< const char* >   myValidationLayers;
    std::vector< const char* >   myDeviceExtensions;
    std::vector< VkImage >       mySwapChainImages;
    std::vector< VkImageView >   mySwapChainImageViews;
    std::vector< VkFramebuffer > mySwapChainFrameBuffers;

    bool     myEnableValidationLayers;
    uint32_t myCurrentFrame = 0;
};
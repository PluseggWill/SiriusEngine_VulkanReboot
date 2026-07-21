#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;

class Vk_ImGuiLayer {
public:
    void Init( GLFWwindow* aWindow, VkInstance anInstance, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice, uint32_t aQueueFamily, VkQueue aQueue,
               VkFormat aSwapchainFormat, VkExtent2D anExtent, const std::vector< VkImageView >& someSwapchainImageViews, uint32_t aImageCount, uint32_t aMinImageCount );
    void Shutdown();

    void NewFrame();
    void Render( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex, VkExtent2D anExtent );

    void DestroySwapchainResources();
    void CreateSwapchainResources( VkFormat aSwapchainFormat, VkExtent2D anExtent, const std::vector< VkImageView >& someSwapchainImageViews, uint32_t aImageCount,
                                   uint32_t aMinImageCount );

private:
    void CreateRenderPass( VkFormat aSwapchainFormat );
    void DestroyRenderPass();
    void CreateFramebuffers( const std::vector< VkImageView >& someSwapchainImageViews, VkExtent2D anExtent );
    void DestroyFramebuffers();
    void InitImGuiBackends( uint32_t aImageCount, uint32_t aMinImageCount );
    void ShutdownImGuiBackends();

    GLFWwindow*                  myWindow          = nullptr;
    VkInstance                   myInstance        = VK_NULL_HANDLE;
    VkPhysicalDevice             myPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice                     myDevice          = VK_NULL_HANDLE;
    uint32_t                     myQueueFamily     = 0;
    VkQueue                      myQueue           = VK_NULL_HANDLE;
    VkFormat                     mySwapchainFormat = VK_FORMAT_UNDEFINED;
    VkRenderPass                 myRenderPass      = VK_NULL_HANDLE;
    std::vector< VkFramebuffer > myFramebuffers;
    bool                         myInitialized = false;
};

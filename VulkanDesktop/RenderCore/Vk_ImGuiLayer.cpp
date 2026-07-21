#include "Vk_ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _MSC_VER
#include <stdlib.h>
#endif

namespace {
void CheckVkResult( VkResult aResult ) {
    if ( aResult != VK_SUCCESS )
        throw std::runtime_error( "Vulkan call failed for ImGui backend." );
}

void ConfigureImGuiPersistence( ImGuiIO& aIO ) {
#if defined( _WIN32 )
    std::string localAppData;
#ifdef _MSC_VER
    char*  envValue = nullptr;
    size_t envLen   = 0;
    if ( _dupenv_s( &envValue, &envLen, "LOCALAPPDATA" ) == 0 && envValue != nullptr ) {
        localAppData.assign( envValue );
        free( envValue );
    }
#else
    if ( const char* envValue = std::getenv( "LOCALAPPDATA" ) ) {
        localAppData = envValue;
    }
#endif
    if ( !localAppData.empty() ) {
        static const std::string iniPath = ( std::filesystem::path( localAppData ) / "SiriusEngine" / "imgui.ini" ).string();
        std::error_code          ec;
        std::filesystem::create_directories( std::filesystem::path( localAppData ) / "SiriusEngine", ec );
        aIO.IniFilename = iniPath.c_str();
    }
#endif
}
}  // namespace

void Vk_ImGuiLayer::Init( GLFWwindow* aWindow, VkInstance anInstance, VkPhysicalDevice aPhysicalDevice, VkDevice aDevice, uint32_t aQueueFamily, VkQueue aQueue,
                          VkFormat aSwapchainFormat, VkExtent2D anExtent, const std::vector< VkImageView >& someSwapchainImageViews, uint32_t aImageCount,
                          uint32_t aMinImageCount ) {
    myWindow          = aWindow;
    myInstance        = anInstance;
    myPhysicalDevice  = aPhysicalDevice;
    myDevice          = aDevice;
    myQueueFamily     = aQueueFamily;
    myQueue           = aQueue;
    mySwapchainFormat = aSwapchainFormat;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureImGuiPersistence( ImGui::GetIO() );
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan( myWindow, true );

    CreateSwapchainResources( mySwapchainFormat, anExtent, someSwapchainImageViews, aImageCount, aMinImageCount );
    myInitialized = true;
}

void Vk_ImGuiLayer::Shutdown() {
    if ( !myInitialized )
        return;

    vkDeviceWaitIdle( myDevice );

    ShutdownImGuiBackends();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    DestroyFramebuffers();
    DestroyRenderPass();

    myInitialized = false;
}

void Vk_ImGuiLayer::NewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Vk_ImGuiLayer::Render( VkCommandBuffer aCommandBuffer, uint32_t anImageIndex, VkExtent2D anExtent ) {
    ImDrawData* drawData = ImGui::GetDrawData();
    if ( drawData == nullptr || drawData->DisplaySize.x <= 0.f || drawData->DisplaySize.y <= 0.f )
        return;

    if ( anImageIndex >= myFramebuffers.size() )
        return;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = myRenderPass;
    renderPassInfo.framebuffer       = myFramebuffers[ anImageIndex ];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = anExtent;

    vkCmdBeginRenderPass( aCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );
    ImGui_ImplVulkan_RenderDrawData( drawData, aCommandBuffer );
    vkCmdEndRenderPass( aCommandBuffer );
}

void Vk_ImGuiLayer::DestroySwapchainResources() {
    if ( !myInitialized )
        return;

    ShutdownImGuiBackends();
    DestroyFramebuffers();
    DestroyRenderPass();
}

void Vk_ImGuiLayer::CreateSwapchainResources( VkFormat aSwapchainFormat, VkExtent2D anExtent, const std::vector< VkImageView >& someSwapchainImageViews, uint32_t aImageCount,
                                              uint32_t aMinImageCount ) {
    mySwapchainFormat = aSwapchainFormat;
    CreateRenderPass( mySwapchainFormat );
    CreateFramebuffers( someSwapchainImageViews, anExtent );
    InitImGuiBackends( aImageCount, aMinImageCount );
}

void Vk_ImGuiLayer::CreateRenderPass( VkFormat aSwapchainFormat ) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = aSwapchainFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if ( vkCreateRenderPass( myDevice, &renderPassInfo, nullptr, &myRenderPass ) != VK_SUCCESS )
        throw std::runtime_error( "failed to create ImGui render pass!" );
}

void Vk_ImGuiLayer::DestroyRenderPass() {
    if ( myRenderPass != VK_NULL_HANDLE ) {
        vkDestroyRenderPass( myDevice, myRenderPass, nullptr );
        myRenderPass = VK_NULL_HANDLE;
    }
}

void Vk_ImGuiLayer::CreateFramebuffers( const std::vector< VkImageView >& someSwapchainImageViews, VkExtent2D anExtent ) {
    DestroyFramebuffers();

    myFramebuffers.resize( someSwapchainImageViews.size() );
    for ( size_t i = 0; i < someSwapchainImageViews.size(); i++ ) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = myRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = &someSwapchainImageViews[ i ];
        framebufferInfo.width           = anExtent.width;
        framebufferInfo.height          = anExtent.height;
        framebufferInfo.layers          = 1;

        if ( vkCreateFramebuffer( myDevice, &framebufferInfo, nullptr, &myFramebuffers[ i ] ) != VK_SUCCESS )
            throw std::runtime_error( "failed to create ImGui framebuffer!" );
    }
}

void Vk_ImGuiLayer::DestroyFramebuffers() {
    for ( VkFramebuffer framebuffer : myFramebuffers )
        vkDestroyFramebuffer( myDevice, framebuffer, nullptr );

    myFramebuffers.clear();
}

void Vk_ImGuiLayer::InitImGuiBackends( uint32_t aImageCount, uint32_t aMinImageCount ) {
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance           = myInstance;
    initInfo.PhysicalDevice     = myPhysicalDevice;
    initInfo.Device             = myDevice;
    initInfo.QueueFamily        = myQueueFamily;
    initInfo.Queue              = myQueue;
    initInfo.DescriptorPool     = VK_NULL_HANDLE;
    initInfo.DescriptorPoolSize = 64;
    initInfo.RenderPass         = myRenderPass;
    initInfo.MinImageCount      = aMinImageCount;
    initInfo.ImageCount         = aImageCount;
    initInfo.MSAASamples        = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn    = CheckVkResult;

    if ( !ImGui_ImplVulkan_Init( &initInfo ) )
        throw std::runtime_error( "ImGui_ImplVulkan_Init failed." );

    if ( !ImGui_ImplVulkan_CreateFontsTexture() )
        throw std::runtime_error( "ImGui_ImplVulkan_CreateFontsTexture failed." );
}

void Vk_ImGuiLayer::ShutdownImGuiBackends() {
    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
}

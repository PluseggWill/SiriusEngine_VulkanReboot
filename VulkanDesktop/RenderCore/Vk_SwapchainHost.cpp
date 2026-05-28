#include "Vk_SwapchainHost.h"

#include "Vk_Core.h"
#include "../Util/Util_Logger.h"

void Vk_SwapchainHost::Init( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Vk_SwapchainHost::Init." );
    aCore.CreateSwapChain();
    aCore.CreateRenderPass();
    aCore.CreateDescriptorSetLayout();
    aCore.CreateColorResources();
    aCore.CreateDepthResources();
    aCore.CreateFrameBuffers();
}

void Vk_SwapchainHost::Recreate( Vk_Core& aCore ) {
    UtilLogger::Info( "SWAPCHAIN", "Vk_SwapchainHost::Recreate." );
    aCore.RecreateSwapChain();
}

#include "Vk_RenderDevice.h"

#include "../Platform/PlatformHost.h"
#include "../Util/Util_Logger.h"
#include "Vk_Bindless.h"
#include "Vk_DevicePipelineCache.h"
#include "Vk_Renderer.h"

void Vk_RenderDevice::Init( Vk_Renderer& aCore, PlatformHost& aPlatformHost ) {
    UtilLogger::Info( "VULKAN", "Vk_RenderDevice::Init: instance/device/allocator." );

    aCore.CreateInstance();
    aPlatformHost.CreateSurface( aCore.myRhi );
    aCore.PickPhysicalDevice();
    aCore.myRhi.myDeviceCtx.myBindlessCaps = Vk_ProbeBindlessCapabilities( aCore.myRhi.myDeviceCtx.myPhysicalDevice, aCore.myRhi.myDeviceCtx.myDeviceExtensions );
    aCore.InitVk_QueueFamilyIndices();
    aCore.CreateLogicalDevice();
    aCore.myRhi.myDeviceCtx.myMaterialPath = Vk_SelectRenderMaterialPath( aCore.myRhi.myDeviceCtx.myBindlessCaps );
    UtilLogger::Info( "BINDLESS", std::string( "materialPath=" ) + Vk_RenderMaterialPathName( aCore.myRhi.myDeviceCtx.myMaterialPath ) );
    // After material path is known (bindless frag included in shader fingerprint when applicable).
    Vk_DevicePipelineCache::Create( aCore );
    aCore.CreateCommandPool();
    aCore.InitAllocator();
}

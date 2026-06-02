#include "Vk_RenderDevice.h"

#include "../Util/Util_Logger.h"
#include "Vk_Bindless.h"
#include "Vk_Core.h"
#include "Vk_DevicePipelineCache.h"

void Vk_RenderDevice::Init( Vk_Core& aCore ) {
    UtilLogger::Info( "VULKAN", "Vk_RenderDevice::Init: instance/device/allocator." );

    aCore.CreateInstance();
    aCore.CreateSurface();
    aCore.PickPhysicalDevice();
    aCore.myDeviceCtx.myBindlessCaps = Vk_ProbeBindlessCapabilities( aCore.myDeviceCtx.myPhysicalDevice, aCore.myDeviceCtx.myDeviceExtensions );
    aCore.InitVk_QueueFamilyIndices();
    aCore.CreateLogicalDevice();
    aCore.myDeviceCtx.myMaterialPath = Vk_SelectRenderMaterialPath( aCore.myDeviceCtx.myBindlessCaps );
    UtilLogger::Info( "BINDLESS", std::string( "materialPath=" ) + Vk_RenderMaterialPathName( aCore.myDeviceCtx.myMaterialPath ) );
    // After material path is known (bindless frag included in shader fingerprint when applicable).
    Vk_DevicePipelineCache::Create( aCore );
    aCore.CreateCommandPool();
    aCore.InitAllocator();
}

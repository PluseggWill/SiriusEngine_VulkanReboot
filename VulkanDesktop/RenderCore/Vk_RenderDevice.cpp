#include "Vk_RenderDevice.h"

#include "Vk_Bindless.h"
#include "Vk_Core.h"
#include "Vk_DevicePipelineCache.h"
#include "../Util/Util_Logger.h"

void Vk_RenderDevice::Init( Vk_Core& aCore ) {
    UtilLogger::Info( "VULKAN", "Vk_RenderDevice::Init: instance/device/allocator." );

    aCore.CreateInstance();
    aCore.CreateSurface();
    aCore.PickPhysicalDevice();
    aCore.myBindlessCaps = Vk_ProbeBindlessCapabilities( aCore.myPhysicalDevice, aCore.myDeviceExtensions );
    aCore.InitVk_QueueFamilyIndices();
    aCore.CreateLogicalDevice();
    aCore.myMaterialPath = Vk_SelectRenderMaterialPath( aCore.myBindlessCaps );
    UtilLogger::Info( "BINDLESS", std::string( "materialPath=" ) + Vk_RenderMaterialPathName( aCore.myMaterialPath ) );
    // After material path is known (bindless frag included in shader fingerprint when applicable).
    Vk_DevicePipelineCache::Create( aCore );
    aCore.CreateCommandPool();
    aCore.InitAllocator();
}

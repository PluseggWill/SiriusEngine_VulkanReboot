#include "Vk_RenderDevice.h"

#include "Vk_Bindless.h"
#include "Vk_Core.h"
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
    aCore.CreateCommandPool();
    aCore.InitAllocator();
}

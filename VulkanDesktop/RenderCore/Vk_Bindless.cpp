// Windows-only dev override: FORCE_MATERIAL_BATCH via _dupenv_s (see Docs/Platform.md).
#include "Vk_Bindless.h"

#include "../Util/Util_Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace {

bool ExtensionAvailable( const char* aName, const std::vector< VkExtensionProperties >& aAvailable ) {
    for ( const VkExtensionProperties& props : aAvailable ) {
        if ( std::strcmp( props.extensionName, aName ) == 0 ) {
            return true;
        }
    }
    return false;
}

void AppendExtensionIfMissing( const char* aName, std::vector< const char* >& aExtensions ) {
    for ( const char* existing : aExtensions ) {
        if ( std::strcmp( existing, aName ) == 0 ) {
            return;
        }
    }
    aExtensions.push_back( aName );
}

}  // namespace

// Instance: VK_KHR_get_physical_device_properties2 required for vkGetPhysicalDeviceFeatures2 (Vulkan 1.0 instance).
void Vk_AppendRequiredInstanceExtensions( std::vector< const char* >& aInstanceExtensions ) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );
    std::vector< VkExtensionProperties > available( extensionCount );
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, available.data() );

    if ( ExtensionAvailable( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, available ) ) {
        AppendExtensionIfMissing( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, aInstanceExtensions );
    }
}

Vk_BindlessCapabilities Vk_ProbeBindlessCapabilities( VkPhysicalDevice aPhysicalDevice, std::vector< const char* >& aDeviceExtensions ) {
    Vk_BindlessCapabilities caps{};

    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties( aPhysicalDevice, &deviceProps );

    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, nullptr );
    std::vector< VkExtensionProperties > available( extensionCount );
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, available.data() );

    // VK_EXT_descriptor_indexing requires properties2 + maintenance3; on 1.2+ GPUs those may be core (not listed as extensions).
    const bool hasIndexingExt  = ExtensionAvailable( VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, available );
    const bool hasProperties2  = deviceProps.apiVersion >= VK_API_VERSION_1_1 || ExtensionAvailable( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, available );
    const bool hasMaintenance3 = deviceProps.apiVersion >= VK_API_VERSION_1_2 || ExtensionAvailable( VK_KHR_MAINTENANCE3_EXTENSION_NAME, available );
    const bool indexingCore    = deviceProps.apiVersion >= VK_API_VERSION_1_2;
    caps.myDescriptorIndexingExtension = hasProperties2 && hasMaintenance3 && ( hasIndexingExt || indexingCore );

    // Only enable extension names still advertised; omit promoted core names to satisfy vkCreateDevice validation.
    if ( caps.myDescriptorIndexingExtension ) {
        if ( ExtensionAvailable( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, available ) ) {
            AppendExtensionIfMissing( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, aDeviceExtensions );
        }
        if ( ExtensionAvailable( VK_KHR_MAINTENANCE3_EXTENSION_NAME, available ) ) {
            AppendExtensionIfMissing( VK_KHR_MAINTENANCE3_EXTENSION_NAME, aDeviceExtensions );
        }
        if ( hasIndexingExt ) {
            AppendExtensionIfMissing( VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, aDeviceExtensions );
        }
    }
    else if ( hasIndexingExt || indexingCore ) {
        UtilLogger::Warn( "BINDLESS", "descriptor indexing unavailable: missing get_physical_device_properties2 or maintenance3 dependency." );
    }

    if ( caps.myDescriptorIndexingExtension ) {
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceFeatures2 features2{};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &indexingFeatures;
        vkGetPhysicalDeviceFeatures2( aPhysicalDevice, &features2 );

        caps.myRuntimeDescriptorArray = indexingFeatures.runtimeDescriptorArray == VK_TRUE;
        caps.myNonUniformIndexing     = indexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
    }

    UtilLogger::Info( "BINDLESS", "descriptorIndexingExt=" + std::string( caps.myDescriptorIndexingExtension ? "yes" : "no" )
                                      + " runtimeArray=" + std::string( caps.myRuntimeDescriptorArray ? "yes" : "no" )
                                      + " nonUniformIndexing=" + std::string( caps.myNonUniformIndexing ? "yes" : "no" ) );

    return caps;
}

Vk_RenderMaterialPath Vk_SelectRenderMaterialPath( const Vk_BindlessCapabilities& aCaps ) {
#ifdef _MSC_VER
    char*         forceBatchEnv = nullptr;
    size_t        envLen        = 0;
    const errno_t envErr        = _dupenv_s( &forceBatchEnv, &envLen, "FORCE_MATERIAL_BATCH" );
    const bool    forceBatch    = envErr == 0 && forceBatchEnv != nullptr && forceBatchEnv[ 0 ] != '\0' && forceBatchEnv[ 0 ] != '0';
    if ( forceBatchEnv != nullptr ) {
        free( forceBatchEnv );
    }
#else
    const char* forceBatchPtr = std::getenv( "FORCE_MATERIAL_BATCH" );
    const bool  forceBatch    = forceBatchPtr != nullptr && forceBatchPtr[ 0 ] != '\0' && forceBatchPtr[ 0 ] != '0';
#endif
    if ( forceBatch ) {
        UtilLogger::Info( "BINDLESS", "FORCE_MATERIAL_BATCH set - using Batch material path." );
        return Vk_RenderMaterialPath::Batch;
    }

    // RenderDoc + bindless has shown startup instability on some driver stacks.
    // Prefer deterministic captures by forcing the simpler Batch material path.
    if ( GetModuleHandleA( "renderdoc.dll" ) != nullptr ) {
        UtilLogger::Info( "BINDLESS", "RenderDoc module detected - forcing Batch material path for capture stability." );
        return Vk_RenderMaterialPath::Batch;
    }

    if ( aCaps.myDescriptorIndexingExtension && aCaps.myRuntimeDescriptorArray && aCaps.myNonUniformIndexing ) {
        return Vk_RenderMaterialPath::Bindless;
    }

    return Vk_RenderMaterialPath::Batch;
}

const char* Vk_RenderMaterialPathName( Vk_RenderMaterialPath aPath ) {
    return aPath == Vk_RenderMaterialPath::Bindless ? "Bindless" : "Batch";
}

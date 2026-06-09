// Windows-only env overrides: FORCE_MATERIAL_BATCH, BINDLESS_RENDERDOC_OK (_dupenv_s; see Docs/Platform.md).
#include "Vk_Bindless.h"

#include "../Util/Util_Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

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

// Non-empty env value that is not "0" counts as true (lab toggles).
bool EnvTruthy( const char* aName ) {
#ifdef _MSC_VER
    char*  value = nullptr;
    size_t len   = 0;
    if ( _dupenv_s( &value, &len, aName ) != 0 || value == nullptr ) {
        return false;
    }
    const bool truthy = value[ 0 ] != '\0' && value[ 0 ] != '0';
    free( value );
    return truthy;
#else
    const char* value = std::getenv( aName );
    return value != nullptr && value[ 0 ] != '\0' && value[ 0 ] != '0';
#endif
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

// POLICY_BINDLESS (Option A): default Bindless when caps OK; Batch is fallback only.
// Maint: Docs/Archived/plans/shader-bindless-policy_Plan.md §Maintenance contract (M1,M5,M6).
Vk_RenderMaterialPath Vk_SelectRenderMaterialPath( const Vk_BindlessCapabilities& aCaps ) {
    if ( EnvTruthy( "FORCE_MATERIAL_BATCH" ) ) {
        UtilLogger::Info( "BINDLESS", "FORCE_MATERIAL_BATCH set - using Batch material path." );
        return Vk_RenderMaterialPath::Batch;
    }

    // POLICY_BINDLESS M5 (#14): default Batch when RenderDoc is injected; opt in with BINDLESS_RENDERDOC_OK=1.
    if ( GetModuleHandleA( "renderdoc.dll" ) != nullptr ) {
        if ( EnvTruthy( "BINDLESS_RENDERDOC_OK" ) ) {
            UtilLogger::Info( "BINDLESS", "RenderDoc detected; BINDLESS_RENDERDOC_OK=1 - keeping bindless path when capable." );
        }
        else {
            UtilLogger::Info( "BINDLESS", "RenderDoc detected - using Batch (set BINDLESS_RENDERDOC_OK=1 to keep bindless)." );
            return Vk_RenderMaterialPath::Batch;
        }
    }

    if ( aCaps.myDescriptorIndexingExtension && aCaps.myRuntimeDescriptorArray && aCaps.myNonUniformIndexing ) {
        return Vk_RenderMaterialPath::Bindless;
    }

    return Vk_RenderMaterialPath::Batch;
}

const char* Vk_RenderMaterialPathName( Vk_RenderMaterialPath aPath ) {
    return aPath == Vk_RenderMaterialPath::Bindless ? "Bindless" : "Batch";
}

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

Vk_BindlessCapabilities Vk_ProbeBindlessCapabilities( VkPhysicalDevice aPhysicalDevice, std::vector< const char* >& aDeviceExtensions ) {
    Vk_BindlessCapabilities caps{};

    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, nullptr );
    std::vector< VkExtensionProperties > available( extensionCount );
    vkEnumerateDeviceExtensionProperties( aPhysicalDevice, nullptr, &extensionCount, available.data() );

    caps.myDescriptorIndexingExtension = ExtensionAvailable( VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, available );
    if ( caps.myDescriptorIndexingExtension ) {
        AppendExtensionIfMissing( VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, aDeviceExtensions );
    }

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &indexingFeatures;
    vkGetPhysicalDeviceFeatures2( aPhysicalDevice, &features2 );

    caps.myRuntimeDescriptorArray = indexingFeatures.runtimeDescriptorArray == VK_TRUE;
    caps.myNonUniformIndexing     = indexingFeatures.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;

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
        UtilLogger::Info( "BINDLESS", "FORCE_MATERIAL_BATCH set — using Batch material path." );
        return Vk_RenderMaterialPath::Batch;
    }

    // RenderDoc + bindless has shown startup instability on some driver stacks.
    // Prefer deterministic captures by forcing the simpler Batch material path.
    if ( GetModuleHandleA( "renderdoc.dll" ) != nullptr ) {
        UtilLogger::Info( "BINDLESS", "RenderDoc module detected — forcing Batch material path for capture stability." );
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

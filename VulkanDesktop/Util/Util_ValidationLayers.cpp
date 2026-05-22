#include "Util_ValidationLayers.h"
#include "Util_Logger.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector< VkLayerProperties > EnumerateInstanceLayers() {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

    std::vector< VkLayerProperties > layers( layerCount );
    if ( layerCount > 0 ) {
        vkEnumerateInstanceLayerProperties( &layerCount, layers.data() );
    }
    return layers;
}

}  // namespace

void UtilValidationLayers::LogInstanceLayerDiscovery() {
    const std::vector< VkLayerProperties > layers = EnumerateInstanceLayers();

    UtilLogger::Info( "VULKAN", "Instance layer discovery (" + std::to_string( layers.size() ) + " available):" );
    for ( const VkLayerProperties& layer : layers ) {
        UtilLogger::Info( "VULKAN", std::string( "  layer " ) + layer.layerName + " spec=" + std::to_string( layer.specVersion ) );
    }
}

bool UtilValidationLayers::AreLayersAvailable( const std::vector< const char* >& aRequestedLayers ) {
    const std::vector< VkLayerProperties > available = EnumerateInstanceLayers();

    for ( const char* requestedName : aRequestedLayers ) {
        bool found = false;
        for ( const VkLayerProperties& layer : available ) {
            if ( std::strcmp( requestedName, layer.layerName ) == 0 ) {
                found = true;
                UtilLogger::Info( "VULKAN", std::string( "Requested validation layer present: " ) + requestedName );
                break;
            }
        }
        if ( !found ) {
            UtilLogger::Error( "VULKAN", std::string( "Requested validation layer missing: " ) + requestedName );
            return false;
        }
    }

    return true;
}

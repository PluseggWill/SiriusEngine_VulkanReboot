#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace UtilValidationLayers {
void LogInstanceLayerDiscovery();

bool AreLayersAvailable( const std::vector< const char* >& aRequestedLayers );
}  // namespace UtilValidationLayers

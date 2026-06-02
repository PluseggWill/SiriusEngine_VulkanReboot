#pragma once

#include <string>
#include <vulkan/vulkan.h>

// VkResult names for logs and fail-fast exceptions.
namespace UtilVulkanResult {
const char* ToString( VkResult aResult );
std::string Describe( VkResult aResult );

void ThrowOnFailure( VkResult aResult, const char* aOperation );
}  // namespace UtilVulkanResult

#pragma once

#include <vulkan/vulkan.h>

namespace UtilDebugMessenger {

// VK_EXT_debug_utils: route validation/layer output to UtilLogger ([VULKAN-VALIDATION]).
bool IsExtensionAvailable();

void PopulateCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& aOut );

// Chains messenger create info on instance create; creates handle after vkCreateInstance.
bool SetupForInstanceCreate( VkInstanceCreateInfo& aCreateInfo );

bool Create( VkInstance aInstance );

void Destroy( VkInstance aInstance );

}  // namespace UtilDebugMessenger

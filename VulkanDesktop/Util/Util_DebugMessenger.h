#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace UtilDebugMessenger {

// VK_EXT_debug_utils: route validation/layer output to UtilLogger ([VULKAN-VALIDATION]).
bool IsExtensionAvailable();

void PopulateCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& aOut );

// Chains messenger create info on instance create; creates handle after vkCreateInstance.
bool SetupForInstanceCreate( VkInstanceCreateInfo& aCreateInfo );

bool Create( VkInstance aInstance );

bool HasActiveMessenger();

void Destroy( VkInstance aInstance );

// Dev/test: capture first validation ERROR message (e.g. descriptor layout mismatch probe).
void BeginValidationErrorCapture();
void EndValidationErrorCapture();
bool TryConsumeCapturedValidationError( std::string& aOutMessage );

}  // namespace UtilDebugMessenger

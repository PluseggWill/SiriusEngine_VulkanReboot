#include "Util_DebugMessenger.h"
#include "Util_Logger.h"

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

VkDebugUtilsMessengerEXT gMessenger = VK_NULL_HANDLE;

PFN_vkCreateDebugUtilsMessengerEXT  gCreateDebugUtilsMessengerEXT  = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT gDestroyDebugUtilsMessengerEXT = nullptr;

VkDebugUtilsMessengerCreateInfoEXT gMessengerCreateInfo{};

bool        gCaptureValidationErrors = false;
std::string gCapturedValidationError;

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT aMessageSeverity, VkDebugUtilsMessageTypeFlagsEXT aMessageTypes,
                                              const VkDebugUtilsMessengerCallbackDataEXT* aCallbackData, void* /*aUserData*/ ) {
    if ( aCallbackData == nullptr ) {
        return VK_FALSE;
    }

    std::ostringstream line;
    if ( aMessageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) {
        line << "validation ";
    }
    if ( aMessageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ) {
        line << "performance ";
    }
    if ( aMessageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ) {
        line << "general ";
    }
    if ( aCallbackData->pMessage != nullptr ) {
        line << aCallbackData->pMessage;
    }

    const std::string message = line.str();
    if ( gCaptureValidationErrors && ( aMessageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) && gCapturedValidationError.empty() ) {
        const bool isError   = ( aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) != 0;
        const bool isWarning = ( aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) != 0;
        if ( isError || isWarning ) {
            gCapturedValidationError = message;
        }
    }
    if ( aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) {
        UtilLogger::Error( "VULKAN-VALIDATION", message );
    }
    else if ( aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) {
        UtilLogger::Warn( "VULKAN-VALIDATION", message );
    }
    else if ( aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) {
        UtilLogger::Info( "VULKAN-VALIDATION", message );
    }
    else {
        UtilLogger::Debug( "VULKAN-VALIDATION", message );
    }

    return VK_FALSE;
}

bool LoadInstanceProcs( VkInstance aInstance ) {
    gCreateDebugUtilsMessengerEXT = reinterpret_cast< PFN_vkCreateDebugUtilsMessengerEXT >(
        vkGetInstanceProcAddr( aInstance, "vkCreateDebugUtilsMessengerEXT" ) );
    gDestroyDebugUtilsMessengerEXT = reinterpret_cast< PFN_vkDestroyDebugUtilsMessengerEXT >(
        vkGetInstanceProcAddr( aInstance, "vkDestroyDebugUtilsMessengerEXT" ) );
    return gCreateDebugUtilsMessengerEXT != nullptr && gDestroyDebugUtilsMessengerEXT != nullptr;
}

}  // namespace

namespace UtilDebugMessenger {

bool IsExtensionAvailable() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, nullptr );

    std::vector< VkExtensionProperties > extensions( extensionCount );
    if ( extensionCount > 0 ) {
        vkEnumerateInstanceExtensionProperties( nullptr, &extensionCount, extensions.data() );
    }

    for ( const VkExtensionProperties& extension : extensions ) {
        if ( std::strcmp( extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0 ) {
            return true;
        }
    }
    return false;
}

void PopulateCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& aOut ) {
    aOut                 = {};
    aOut.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    aOut.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    aOut.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    aOut.pfnUserCallback = DebugCallback;
}

bool SetupForInstanceCreate( VkInstanceCreateInfo& aCreateInfo ) {
    if ( !IsExtensionAvailable() ) {
        UtilLogger::Warn( "VULKAN", std::string( "Extension not available: " ) + VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                                         + " (validation messages may not reach engine_runtime_log.txt)." );
        return false;
    }

    PopulateCreateInfo( gMessengerCreateInfo );
    gMessengerCreateInfo.pNext = aCreateInfo.pNext;
    aCreateInfo.pNext          = &gMessengerCreateInfo;
    UtilLogger::Info( "VULKAN", "Debug utils messenger create info chained on VkInstanceCreateInfo." );
    return true;
}

bool HasActiveMessenger() {
    return gMessenger != VK_NULL_HANDLE;
}

bool Create( VkInstance aInstance ) {
    if ( !LoadInstanceProcs( aInstance ) ) {
        UtilLogger::Warn( "VULKAN", "vkCreateDebugUtilsMessengerEXT not available; messenger handle not created." );
        return false;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateCreateInfo( createInfo );

    if ( gCreateDebugUtilsMessengerEXT( aInstance, &createInfo, nullptr, &gMessenger ) != VK_SUCCESS ) {
        UtilLogger::Warn( "VULKAN", "vkCreateDebugUtilsMessengerEXT failed." );
        gMessenger = VK_NULL_HANDLE;
        return false;
    }

    UtilLogger::Info( "VULKAN", "Debug utils messenger created." );
    return true;
}

void Destroy( VkInstance aInstance ) {
    if ( gMessenger == VK_NULL_HANDLE || gDestroyDebugUtilsMessengerEXT == nullptr ) {
        return;
    }

    gDestroyDebugUtilsMessengerEXT( aInstance, gMessenger, nullptr );
    gMessenger = VK_NULL_HANDLE;
    UtilLogger::Info( "VULKAN", "Debug utils messenger destroyed." );
}

void BeginValidationErrorCapture() {
    gCaptureValidationErrors   = true;
    gCapturedValidationError.clear();
}

void EndValidationErrorCapture() {
    gCaptureValidationErrors = false;
}

bool TryConsumeCapturedValidationError( std::string& aOutMessage ) {
    if ( gCapturedValidationError.empty() ) {
        return false;
    }
    aOutMessage = gCapturedValidationError;
    gCapturedValidationError.clear();
    return true;
}

}  // namespace UtilDebugMessenger

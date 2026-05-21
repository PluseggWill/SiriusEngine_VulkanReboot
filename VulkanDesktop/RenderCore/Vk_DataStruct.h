#pragma once
#include <deque>
#include <functional>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vk_QueueFamilyIndices {
    std::optional< uint32_t > myGraphicsFamily;
    std::optional< uint32_t > myPresentFamily;
    std::optional< uint32_t > myTransferFamily;

    bool isComplete() const {
        return myGraphicsFamily.has_value() && myPresentFamily.has_value();
    }

    // Graphics queues support transfer; use them when no dedicated transfer-only family exists.
    void ApplyTransferFallback() {
        if ( !myTransferFamily.has_value() && myGraphicsFamily.has_value() ) {
            myTransferFamily = myGraphicsFamily;
        }
    }
};

struct Vk_SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          myCapabilities;
    std::vector< VkSurfaceFormatKHR > myFormats;
    std::vector< VkPresentModeKHR >   myPresentModes;
};

struct Vk_DeletionQueue {
    std::deque< std::function< void() > > myDeletors;

    void pushFunction( std::function< void() >&& aFunction ) {
        myDeletors.push_back( aFunction );
    }

    void flush() {
        // FILO order
        for ( auto it = myDeletors.rbegin(); it != myDeletors.rend(); it++ ) {
            ( *it )();
        }

        myDeletors.clear();
    }
};
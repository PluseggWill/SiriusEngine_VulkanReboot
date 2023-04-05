#pragma once
#include <deque>
#include <functional>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct QueueFamilyIndices {
    std::optional< uint32_t > myGraphicsFamily;
    std::optional< uint32_t > myPresentFamily;
    std::optional< uint32_t > myTransferFamily;

    bool isComplete() const {
        return myGraphicsFamily.has_value() && myPresentFamily.has_value() && myTransferFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          myCapabilities;
    std::vector< VkSurfaceFormatKHR > myFormats;
    std::vector< VkPresentModeKHR >   myPresentModes;
};

struct DeletionQueue {
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
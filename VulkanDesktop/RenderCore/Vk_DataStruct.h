#pragma once
#include <deque>
#include <functional>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Queue families required to create the logical device and command pools.
struct Vk_QueueFamilyIndices {
    std::optional< uint32_t > myGraphicsFamily;  // draw + (usually) same index as present
    std::optional< uint32_t > myPresentFamily;
    std::optional< uint32_t > myTransferFamily;  // optional dedicated COPY queue

    // Ready to create device: graphics + present. Transfer may be filled by ApplyTransferFallback().
    bool isComplete() const {
        return myGraphicsFamily.has_value() && myPresentFamily.has_value();
    }

    // Many GPUs expose TRANSFER on the graphics family only. Duplicate index is valid.
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

// Deferred destruction: push lambdas at create time, flush in reverse order (FILO).
struct Vk_DeletionQueue {
    std::deque< std::function< void() > > myDeletors;

    void pushFunction( std::function< void() >&& aFunction ) {
        myDeletors.push_back( aFunction );
    }

    void flush() {
        // Destroy in reverse registration order (children before parents).
        for ( auto it = myDeletors.rbegin(); it != myDeletors.rend(); it++ ) {
            ( *it )();
        }

        myDeletors.clear();
    }

    // Drop the oldest deferred deletor without running it (swapchain recreate keeps WSI handle for oldSwapchain).
    void popFront() {
        if ( !myDeletors.empty() ) {
            myDeletors.pop_front();
        }
    }
};

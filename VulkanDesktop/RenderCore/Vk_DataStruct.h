#pragma once
#include <deque>
#include <functional>
#include <optional>

#include "Vk_Types.h"

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

// Keep in mind that Vulkan expects the data to be aligned, see https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout for more details
struct UniformBufferObject {
    alignas( 16 ) glm::vec2 test;
    alignas( 16 ) glm::mat4 model;
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
};

// TODO: Instance rendering

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
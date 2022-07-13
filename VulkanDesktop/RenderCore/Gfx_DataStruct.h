#pragma once
#include <array>
#include <deque>
#include <functional>
#include <optional>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#endif

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

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

struct AllocatedImage {
    VkImage        myImage;
    VkDeviceMemory myMemory;
};

struct AllocatedBuffer {
    VkBuffer       myBuffer;
    VkDeviceMemory myMemory;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof( Vertex );
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array< VkVertexInputAttributeDescription, 3 > getAttributeDescriptions() {
        std::array< VkVertexInputAttributeDescription, 3 > attributeDescriptions{};
        attributeDescriptions[ 0 ].binding  = 0;
        attributeDescriptions[ 0 ].location = 0;
        attributeDescriptions[ 0 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 0 ].offset   = offsetof( Vertex, pos );

        attributeDescriptions[ 1 ].binding  = 0;
        attributeDescriptions[ 1 ].location = 1;
        attributeDescriptions[ 1 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 1 ].offset   = offsetof( Vertex, color );

        attributeDescriptions[ 2 ].binding  = 0;
        attributeDescriptions[ 2 ].location = 2;
        attributeDescriptions[ 2 ].format   = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[ 2 ].offset   = offsetof( Vertex, texCoord );

        return attributeDescriptions;
    }

    bool operator==( const Vertex& other ) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

// Hash function for Vertex struct
namespace std {
template <> struct hash< Vertex > {
    size_t operator()( Vertex const& vertex ) const {
        return ( ( hash< glm::vec3 >()( vertex.pos ) ^ ( hash< glm::vec3 >()( vertex.color ) << 1 ) ) >> 1 ) ^ ( hash< glm::vec2 >()( vertex.texCoord ) << 1 );
    }
};
}  // namespace std

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
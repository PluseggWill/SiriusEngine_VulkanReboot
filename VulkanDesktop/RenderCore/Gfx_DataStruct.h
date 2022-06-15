#pragma once
#include "../Util/Util_Include.h"

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

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;
        bindingDescription.stride    = sizeof( Vertex );
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array< VkVertexInputAttributeDescription, 2 > getAttributeDescriptions() {
        std::array< VkVertexInputAttributeDescription, 2 > attributeDescriptions{};
        attributeDescriptions[ 0 ].binding  = 0;
        attributeDescriptions[ 0 ].location = 0;
        attributeDescriptions[ 0 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 0 ].offset   = offsetof( Vertex, pos );
        attributeDescriptions[ 1 ].binding  = 0;
        attributeDescriptions[ 1 ].location = 1;
        attributeDescriptions[ 1 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[ 1 ].offset   = offsetof( Vertex, color );

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// TODO: Instance rendering
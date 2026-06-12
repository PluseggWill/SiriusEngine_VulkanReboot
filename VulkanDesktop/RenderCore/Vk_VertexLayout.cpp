#include "Vk_VertexLayout.h"

#include <cstddef>

VkVertexInputBindingDescription Vk_GetGfxVertexBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof( Gfx_Vertex );
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array< VkVertexInputAttributeDescription, 4 > Vk_GetGfxVertexAttributeDescriptions() {
    std::array< VkVertexInputAttributeDescription, 4 > attributeDescriptions{};
    attributeDescriptions[ 0 ].binding  = 0;
    attributeDescriptions[ 0 ].location = 0;
    attributeDescriptions[ 0 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 0 ].offset   = offsetof( Gfx_Vertex, pos );

    attributeDescriptions[ 1 ].binding  = 0;
    attributeDescriptions[ 1 ].location = 1;
    attributeDescriptions[ 1 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 1 ].offset   = offsetof( Gfx_Vertex, color );

    attributeDescriptions[ 2 ].binding  = 0;
    attributeDescriptions[ 2 ].location = 2;
    attributeDescriptions[ 2 ].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[ 2 ].offset   = offsetof( Gfx_Vertex, texCoord );

    attributeDescriptions[ 3 ].binding  = 0;
    attributeDescriptions[ 3 ].location = 3;
    attributeDescriptions[ 3 ].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[ 3 ].offset   = offsetof( Gfx_Vertex, normal );

    return attributeDescriptions;
}

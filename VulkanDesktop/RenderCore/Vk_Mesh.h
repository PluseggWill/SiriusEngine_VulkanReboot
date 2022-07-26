#pragma once
#include <array>

#include "Vk_Types.h"

class Vk_Core;

class Vertex {
public:
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array< VkVertexInputAttributeDescription, 3 > getAttributeDescriptions();

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

class Mesh {
public:
    std::vector< Vertex >   myVertices;
    std::vector< uint32_t > myIndices;

    AllocatedBuffer myVertexBuffer;
    AllocatedBuffer myIndexBuffer;

public:
    void LoadMesh( const std::string& aPath );
    void BuildBuffers();
    void BuildVertexBuffer();
    void BuildIndexBuffer();
};
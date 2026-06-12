#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// API-agnostic vertex layout — Vulkan input descriptions live in RenderCore/Vk_VertexLayout.cpp.
class Gfx_Vertex {
public:
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;

    bool operator==( const Gfx_Vertex& other ) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
    }
};

namespace std {
template <> struct hash< Gfx_Vertex > {
    size_t operator()( Gfx_Vertex const& vertex ) const {
        return ( ( ( hash< glm::vec3 >()( vertex.pos ) ^ ( hash< glm::vec3 >()( vertex.color ) << 1 ) ) >> 1 ) ^ ( hash< glm::vec2 >()( vertex.texCoord ) << 1 ) )
               ^ ( hash< glm::vec3 >()( vertex.normal ) << 1 );
    }
};
}  // namespace std

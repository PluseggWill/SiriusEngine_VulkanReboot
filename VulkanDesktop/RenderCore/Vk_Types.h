#pragma once
#include <string>
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vk_AllocatedImage {
public:
    VkImage       myImage;
    VmaAllocation myAllocation;
};

struct Vk_AllocatedBuffer {
public:
    VkBuffer      myBuffer;
    VmaAllocation myAllocation;
};

struct Gfx_Texture {
private:
    Vk_AllocatedImage myAllocImage;
    VkImageView    myImageView;

public:
    VkImage& Image() {
        return myAllocImage.myImage;
    }

    VmaAllocation& Allocation() {
        return myAllocImage.myAllocation;
    }

    VkImageView& ImageView() {
        return myImageView;
    }

    Vk_AllocatedImage& AllocImage() {
        return myAllocImage;
    }
};

struct Gfx_Material {
    VkPipeline       myPipeline;
    VkPipelineLayout myPipelineLayout;
};

class Gfx_Vertex {
public:
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array< VkVertexInputAttributeDescription, 4 > getAttributeDescriptions();

    bool operator==( const Gfx_Vertex& other ) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
    }
};

// Hash function for Gfx_Vertex struct
namespace std {
template <> struct hash< Gfx_Vertex > {
    size_t operator()( Gfx_Vertex const& vertex ) const {
        return ( ( ( hash< glm::vec3 >()( vertex.pos ) ^ ( hash< glm::vec3 >()( vertex.color ) << 1 ) ) >> 1 ) ^ ( hash< glm::vec2 >()( vertex.texCoord ) << 1 ) ) ^
               ( hash< glm::vec3 >()( vertex.normal ) << 1 );
    }
};
}  // namespace std

class Gfx_Mesh {
public:
    std::vector< Gfx_Vertex >   myVertices;
    std::vector< uint32_t > myIndices;

    Vk_AllocatedBuffer myVertexBuffer;
    Vk_AllocatedBuffer myIndexBuffer;

public:
    void LoadMesh( const std::string& aPath );
    void BuildBuffers();
    void BuildVertexBuffer();
    void BuildIndexBuffer();
};

class Gfx_RenderObject {
public:
    Gfx_Mesh*     myMesh;
    Gfx_Material* myMaterial;
    glm::mat4 myTransform;
};

// std140 UBO, binding eVk_EnvBinding — field order must match EnvironmentData in TriangleFrag_Lit.frag.
struct GpuEnvironmentData {
    glm::vec4 myFogColor;           // reserved (fog not implemented in shader)
    glm::vec4 myFogDistance;        // REPURPOSED: x=specularStrength, y=shininess, z=textureBlend, w unused
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;  // xyz = direction from surface toward sun (normalized each frame)
    glm::vec4 mySunlightColor;
    glm::vec4 myViewWorldPos;       // xyz = camera world position (specular view vector)
};
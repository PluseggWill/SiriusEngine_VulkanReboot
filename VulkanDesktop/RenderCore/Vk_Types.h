#pragma once
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct AllocatedImage {
public:
    VkImage       myImage;
    VmaAllocation myAllocation;
};

struct AllocatedBuffer {
public:
    VkBuffer      myBuffer;
    VmaAllocation myAllocation;
};

struct Texture {
private:
    AllocatedImage myAllocImage;
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

    AllocatedImage& AllocImage() {
        return myAllocImage;
    }
};

struct Material {
    VkPipeline       myPipeline;
    VkPipelineLayout myPipelineLayout;
};

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

class RenderObject {
public:
    Mesh*     myMesh;
    Material* myMaterial;
    glm::mat4 myTransform;
};

class Gfx_EnvironmentData {
public:
    // 4 * 4 * 5 = 80 bytes
    glm::vec4 myFogColor;
    glm::vec4 myFogDistance;
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;
    glm::vec4 mySunlightColor;
};
#pragma once
#include <cstddef>
#include <string>
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vk_ResourceContext;

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
    VkImageView       myImageView;

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

    const VkImageView& ImageView() const {
        return myImageView;
    }

    Vk_AllocatedImage& AllocImage() {
        return myAllocImage;
    }
};

// Alpha mode for forward materials (matches MaterialData.alphaMode in lit shaders).
enum Gfx_MaterialAlphaMode : uint32_t {
    Gfx_MaterialAlphaMode_Opaque = 0,
    Gfx_MaterialAlphaMode_Mask   = 1,
    Gfx_MaterialAlphaMode_Blend  = 2,
};

// Forward debug visualization (packed in GpuEnvironmentData.myFogDistance.w for lit shaders).
enum Gfx_DebugViewMode : uint32_t {
    Gfx_DebugViewMode_Lit          = 0,
    Gfx_DebugViewMode_Depth        = 1,
    Gfx_DebugViewMode_WorldNormal  = 2,
};

inline float Gfx_DebugViewModeToShaderPacked( Gfx_DebugViewMode aMode ) {
    return static_cast< float >( aMode );
}

struct Gfx_Material {
    VkPipeline       myPipeline;
    VkPipelineLayout myPipelineLayout;
    glm::vec4        myBaseColorFactor{ 1.0f };
    float            myRoughness       = 0.5f;
    float            myMetallic        = 0.0f;
    float            myAlpha           = 1.0f;
    uint32_t         myAlphaMode       = Gfx_MaterialAlphaMode_Opaque;
    bool             myIsTransparent   = false;
};

// std140, Set 1 binding 1 — must match MaterialData in TriangleFrag_Lit.frag (Stage 1 forward contract).
struct GpuMaterialParams {
    alignas( 16 ) glm::vec4 myBaseColorFactor{ 1.0f };
    alignas( 4 ) float      myRoughness = 0.5f;
    alignas( 4 ) float      myMetallic  = 0.0f;
    alignas( 4 ) float      myAlpha     = 1.0f;
    alignas( 4 ) uint32_t   myAlphaMode = Gfx_MaterialAlphaMode_Opaque;
};

// std430 material table entry — must match GpuMaterialEntry in TriangleFrag_Lit_Bindless.frag (vec4 @ 32).
struct GpuMaterialTableEntry {
    uint32_t  myTextureIndex = 0;
    float     myRoughness    = 0.5f;
    float     myMetallic     = 0.0f;
    float     myAlpha        = 1.0f;
    uint32_t  myAlphaMode    = Gfx_MaterialAlphaMode_Opaque;
    uint32_t  _padStd430[ 3 ]{};  // std430: vec4 baseColorFactor starts at byte 32
    glm::vec4 myBaseColorFactor{ 1.0f };
};
static_assert( sizeof( GpuMaterialTableEntry ) == 48, "GpuMaterialTableEntry std430 size" );
static_assert( offsetof( GpuMaterialTableEntry, myBaseColorFactor ) == 32, "GpuMaterialTableEntry std430 vec4 offset" );

inline GpuMaterialParams Gfx_MaterialToGpuParams( const Gfx_Material& aMaterial ) {
    GpuMaterialParams params{};
    params.myBaseColorFactor = aMaterial.myBaseColorFactor;
    params.myRoughness      = aMaterial.myRoughness;
    params.myMetallic       = aMaterial.myMetallic;
    params.myAlpha          = aMaterial.myAlpha;
    params.myAlphaMode       = aMaterial.myAlphaMode;
    return params;
}

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
        return ( ( ( hash< glm::vec3 >()( vertex.pos ) ^ ( hash< glm::vec3 >()( vertex.color ) << 1 ) ) >> 1 ) ^ ( hash< glm::vec2 >()( vertex.texCoord ) << 1 ) )
               ^ ( hash< glm::vec3 >()( vertex.normal ) << 1 );
    }
};
}  // namespace std

class Gfx_Mesh {
public:
    std::vector< Gfx_Vertex > myVertices;
    std::vector< uint32_t >   myIndices;

    Vk_AllocatedBuffer myVertexBuffer;
    Vk_AllocatedBuffer myIndexBuffer;

public:
    void LoadMesh( const std::string& aPath );
    void BuildBuffers( const Vk_ResourceContext& aContext );
    void BuildVertexBuffer( const Vk_ResourceContext& aContext );
    void BuildIndexBuffer( const Vk_ResourceContext& aContext );
};

class Gfx_RenderObject {
public:
    Gfx_Mesh*     myMesh;
    Gfx_Material* myMaterial;
    glm::mat4     myTransform;
};

// std140 UBO, binding eVk_EnvBinding - field order must match EnvironmentData in TriangleFrag_Lit.frag.
struct GpuEnvironmentData {
    glm::vec4 myFogColor;     // reserved (fog not implemented in shader)
    glm::vec4 myFogDistance;  // x=specularStrength, y=shininess, z=textureBlend, w=Gfx_DebugViewMode (as float)
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;  // xyz = direction from surface toward sun (normalized each frame)
    glm::vec4 mySunlightColor;
    glm::vec4 myViewWorldPos;  // xyz = camera world position (specular view vector)
};
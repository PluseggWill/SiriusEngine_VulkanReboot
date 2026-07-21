#pragma once

// GPU-resident mesh/material/texture tables. CPU geometry: Gfx_MeshCpu.

#include "../Gfx/Gfx_MaterialTypes.h"
#include "../Gfx/Gfx_MeshCpu.h"
#include "Vk_AllocatedResource.h"

#include <glm/glm.hpp>

struct Vk_ResourceContext;

struct Vk_TextureResource {
    Vk_AllocatedImage myAllocImage{};
    VkImageView       myImageView = VK_NULL_HANDLE;

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

struct Vk_MaterialResource {
    VkPipeline       myPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout myPipelineLayout = VK_NULL_HANDLE;
    glm::vec4        myBaseColorFactor{ 1.0f };
    float            myRoughness     = 0.5f;
    float            myMetallic      = 0.0f;
    float            myAlpha         = 1.0f;
    uint32_t         myAlphaMode     = Gfx_MaterialAlphaMode_Opaque;
    bool             myIsTransparent = false;
};

inline Gpu_MaterialParams Vk_MaterialResourceToGpuParams( const Vk_MaterialResource& aMaterial ) {
    Gpu_MaterialParams params{};
    params.myBaseColorFactor = aMaterial.myBaseColorFactor;
    params.myRoughness       = aMaterial.myRoughness;
    params.myMetallic        = aMaterial.myMetallic;
    params.myAlpha           = aMaterial.myAlpha;
    params.myAlphaMode       = aMaterial.myAlphaMode;
    return params;
}

struct Vk_MeshResource {
    Gfx_MeshCpu        myCpu{};
    uint32_t           myIndexCount = 0;
    Vk_AllocatedBuffer myVertexBuffer{};
    Vk_AllocatedBuffer myIndexBuffer{};

    const Gfx_Bounds& myLocalBounds() const {
        return myCpu.myLocalBounds;
    }

    void BuildGpuBuffers( const Vk_ResourceContext& aContext );
    void BuildVertexBuffer( const Vk_ResourceContext& aContext );
    void BuildIndexBuffer( const Vk_ResourceContext& aContext );
};

struct Vk_RenderObject {
    Vk_MeshResource*     myMesh     = nullptr;
    Vk_MaterialResource* myMaterial = nullptr;
    glm::mat4            myTransform{ 1.0f };
};

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// CPU-only asset manifest for resource table load (no Vulkan).
// Production path: Gfx_BuildResourceManifestFromSceneDesc (scene-load Phase C).

struct Gfx_MeshManifestEntry {
    uint32_t    myId = 0;
    std::string myPath;
};

struct Gfx_TextureManifestEntry {
    uint32_t    myId = 0;
    std::string myPath;
};

struct Gfx_MaterialManifestEntry {
    uint32_t  myId                    = 0;
    uint32_t  myTextureId             = 0;
    uint32_t  myPipelinePermutationId = 0;
    glm::vec4 myBaseColorFactor{ 1.0f };
    float     myRoughness     = 0.5f;
    float     myMetallic      = 0.0f;
    float     myAlpha         = 1.0f;
    uint32_t  myAlphaMode     = 0;
    bool      myIsTransparent = false;
};

struct Gfx_ResourceManifest {
    std::vector< Gfx_MeshManifestEntry >     myMeshes;
    std::vector< Gfx_TextureManifestEntry >  myTextures;
    std::vector< Gfx_MaterialManifestEntry > myMaterials;
};

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// CPU-only asset manifest for resource table load (no Vulkan).
// Production path: Gfx_BuildResourceManifestFromSceneDesc (scene-load Phase C).

struct Gfx_MeshManifestEntry {
    uint32_t    myId   = 0;
    std::string myPath;
};

struct Gfx_TextureManifestEntry {
    uint32_t    myId   = 0;
    std::string myPath;
};

struct Gfx_MaterialManifestEntry {
    uint32_t myId                    = 0;
    uint32_t myTextureId             = 0;
    uint32_t myPipelinePermutationId = 0;
    float    myAlpha                 = 1.0f;
    bool     myIsTransparent         = false;
};

struct Gfx_ResourceManifest {
    std::vector< Gfx_MeshManifestEntry >     myMeshes;
    std::vector< Gfx_TextureManifestEntry >  myTextures;
    std::vector< Gfx_MaterialManifestEntry > myMaterials;
};

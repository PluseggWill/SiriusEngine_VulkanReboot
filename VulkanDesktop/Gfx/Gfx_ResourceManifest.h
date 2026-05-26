#pragma once

#include <cstdint>
#include <string>
#include <vector>

// CPU-only asset manifest for resource table load (no Vulkan).
// v0: Gfx_BuildDemoResourceManifest mirrors UtilDemoAssets until scene-load Phase C JSON drives this.

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

void Gfx_BuildDemoResourceManifest( Gfx_ResourceManifest& aOut );

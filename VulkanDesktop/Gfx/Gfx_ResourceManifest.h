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

// LEGACY (not called at runtime after scene-load Phase C): reference builder for the Kenney demo manifest.
// Kept so manifest layout and dense id assignment stay documented in code without parsing JSON; useful for
// diffing against Gfx_BuildResourceManifestFromSceneDesc output and for future headless tests. Remove when
// Gfx_BuildDemoLodTable is scene-driven and nothing references UtilDemoAssets manifest ids.
void Gfx_BuildDemoResourceManifest( Gfx_ResourceManifest& aOut );

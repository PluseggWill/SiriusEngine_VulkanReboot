#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Gfx_SceneSoA.h"

// Canonical on-disk scene description (AoS, string ids). Parsed from Data/Scenes/*.json.
// Hydration into Gfx_SceneSoA + resource table ids is scene-load Phase C (not here).

inline constexpr uint32_t kGfxSceneFormatVersion = 1;
inline constexpr const char* kGfxDefaultSceneLogicalPath = "Data/Scenes/demo.json";

struct Gfx_SceneShaderPair {
    std::string myVertPath;
    std::string myFragPath;
};

struct Gfx_SceneMeshEntry {
    std::string myId;
    std::string myPath;
};

struct Gfx_SceneTextureEntry {
    std::string myId;
    std::string myPath;
};

struct Gfx_SceneMaterialEntry {
    std::string myId;
    std::string myShaderId;
    std::string myTextureId;
    float       myAlpha         = 1.0f;
    bool        myIsTransparent = false;
};

struct Gfx_SceneLogicalMeshEntry {
    std::string              myId;
    std::vector< std::string > myLodMeshes;
    std::vector< float >     myLodDistances;
};

struct Gfx_SceneEntityEntry {
    std::string     myLogicalMeshId;  // Key in Gfx_SceneLogicalMeshEntry::myId → SoA logical mesh index.
    std::string     myMaterialId;
    glm::mat4       myTransform{ 1.0f };
    Gfx_RenderFlags myRenderFlags = Gfx_RenderOpaque;
    uint32_t        myLayerMask   = 0xFFFFFFFFu;
    float           myLodBias     = 0.0f;
};

struct Gfx_SceneDesc {
    uint32_t myVersion = 0;
    std::string myName;
    std::unordered_map< std::string, Gfx_SceneShaderPair > myShaders;
    std::vector< Gfx_SceneLogicalMeshEntry > myLogicalMeshes;
    std::vector< Gfx_SceneMeshEntry > myMeshes;
    std::vector< Gfx_SceneTextureEntry > myTextures;
    std::vector< Gfx_SceneMaterialEntry > myMaterials;
    std::vector< Gfx_SceneEntityEntry > myEntities;
};

// Stable string-id → dense table index maps (array order in scene JSON).
struct Gfx_SceneIdTables {
    std::unordered_map< std::string, uint32_t > myMeshIdByName;
    std::unordered_map< std::string, uint32_t > myTextureIdByName;
    std::unordered_map< std::string, uint32_t > myMaterialIdByName;
    std::unordered_map< std::string, uint32_t > myLogicalMeshIdByName;
};

Gfx_SceneIdTables Gfx_BuildSceneIdTables( const Gfx_SceneDesc& aScene );

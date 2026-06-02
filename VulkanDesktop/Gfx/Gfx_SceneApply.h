#pragma once

#include "Gfx_Lod.h"
#include "Gfx_ResourceManifest.h"
#include "Gfx_SceneDesc.h"
#include "Gfx_SceneSoA.h"
#include "Gfx_SceneTransform.h"

#include <string>

// Hydrate runtime structures from a parsed Gfx_SceneDesc (CPU-only; no Vulkan).

Gfx_SceneShaderPair Gfx_GetSceneShader( const Gfx_SceneDesc& aScene, const std::string& aShaderId );

void Gfx_BuildResourceManifestFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_ResourceManifest& aOut );

void Gfx_BuildLodTableFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_LodTable& aOut );

void Gfx_PopulateSceneSoAFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_SceneSoA& aSceneSoA, Gfx_SceneTransformState& aTransformState );

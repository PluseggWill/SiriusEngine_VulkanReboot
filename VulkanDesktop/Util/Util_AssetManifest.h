#pragma once

#include "../Gfx/Gfx_SceneDesc.h"

#include <cstdint>
#include <string>
#include <vector>

// Asset path closure derived from Gfx_SceneDesc (CPU-only, no Vulkan).
// Util_VerifyManifest runs before Vulkan init; Phase C maps paths into resource tables.

enum class Util_AssetKind : uint8_t {
    ShaderVert = 0,
    ShaderFrag,
    Mesh,
    Texture,
};

struct Util_AssetManifestEntry {
    std::string    myLogicalPath;
    Util_AssetKind myKind = Util_AssetKind::Mesh;
};

struct Util_AssetManifest {
    std::vector< Util_AssetManifestEntry > myEntries;
};

// Union of all shader/mesh/texture paths referenced by scene resource tables (+ material shader/texture refs).
Util_AssetManifest Util_CollectDependencies( const Gfx_SceneDesc& aScene );

// Convenience: sorted unique repo-relative paths (same order as Util_CollectDependencies entries).
std::vector< std::string > Util_CollectDependencyPaths( const Gfx_SceneDesc& aScene );

// Fail-fast file existence check before Vulkan init (strict; logs [STARTUP]).
void Util_VerifyManifest( const Util_AssetManifest& aManifest );

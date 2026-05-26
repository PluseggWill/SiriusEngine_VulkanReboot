#pragma once

#include <cstdint>
#include <string_view>

// Stable numeric ids for demo content (logical/physical mesh, material indices).
// Paths and entities live in Data/Scenes/demo.json; runtime uses Gfx_BuildResourceManifestFromSceneDesc.
// Still used by Gfx_BuildDemoResourceManifest (reference) and Gfx_BuildDemoLodTable (until scene JSON LOD only).

namespace UtilDemoAssets {
inline constexpr std::string_view kVertSpv = "VulkanDesktop/Shader_Generated/TriangleVert.spv";
inline constexpr std::string_view kFragSpv = "VulkanDesktop/Shader_Generated/TrianglePix.spv";

inline constexpr std::string_view kDemoTexture = "Data/Textures/viking_room.png";
inline constexpr std::string_view kAltTexture  = "Data/Textures/RedMoon.jpg";
inline constexpr std::string_view kTexRock     = "Data/Textures/ph_rock_diff_1k.jpg";
inline constexpr std::string_view kTexGrass    = "Data/Textures/ph_grass_diff_1k.jpg";
inline constexpr std::string_view kTexMetal    = "Data/Textures/ph_metal_diff_1k.jpg";
inline constexpr std::string_view kTexWood     = "Data/Textures/ph_wood_table_diff_1k.jpg";

inline constexpr std::string_view kHouseModel         = "Data/Models/viking_room.obj";
inline constexpr std::string_view kMonkeyModel        = "Data/Models/monkey_smooth.obj";
inline constexpr std::string_view kKenneyTreeDetailed = "Data/Models/kenney_tree_detailed.obj";
inline constexpr std::string_view kKenneyTreeSimple   = "Data/Models/kenney_tree_simple.obj";
inline constexpr std::string_view kKenneyRockLarge    = "Data/Models/kenney_rock_large_a.obj";
inline constexpr std::string_view kKenneyCampfire     = "Data/Models/kenney_campfire_stones.obj";
inline constexpr std::string_view kKenneyTent         = "Data/Models/kenney_tent_closed.obj";
inline constexpr std::string_view kKenneyStump        = "Data/Models/kenney_stump_round.obj";

// Logical mesh ids (SoA + Gfx_LodTable). Physical mesh ids below are resource-table indices.
inline constexpr uint32_t kLogicalViking   = 0;
inline constexpr uint32_t kLogicalMonkey   = 1;
inline constexpr uint32_t kLogicalTree     = 2;
inline constexpr uint32_t kLogicalRock     = 3;
inline constexpr uint32_t kLogicalCampfire = 4;
inline constexpr uint32_t kLogicalTent     = 5;
inline constexpr uint32_t kLogicalStump    = 6;

// Physical mesh ids (Gfx_BuildDemoResourceManifest).
inline constexpr uint32_t kMeshViking       = 0;
inline constexpr uint32_t kMeshMonkey       = 1;
inline constexpr uint32_t kMeshTreeDetailed = 2;
inline constexpr uint32_t kMeshTreeSimple   = 3;
inline constexpr uint32_t kMeshRock         = 4;
inline constexpr uint32_t kMeshCampfire     = 5;
inline constexpr uint32_t kMeshTent         = 6;
inline constexpr uint32_t kMeshStump        = 7;

inline constexpr uint32_t kMatViking       = 0;
inline constexpr uint32_t kMatMonkey       = 1;
inline constexpr uint32_t kMatTransparent  = 2;
inline constexpr uint32_t kMatRock         = 3;
inline constexpr uint32_t kMatGrass        = 4;
inline constexpr uint32_t kMatMetal        = 5;
inline constexpr uint32_t kMatWood         = 6;

}  // namespace UtilDemoAssets

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// Repo-relative paths for the current demo scene (Vk_Core). Temporary until scene-load (Docs/scene-load_Plan.md).
// Keep in sync with Gfx_BuildDemoResourceManifest and UtilStartupChecks::VerifyRequiredAssets.

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

// Mesh / texture / material ids (Gfx_BuildDemoResourceManifest).
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

inline constexpr std::array< std::string_view, 16 > kRequiredFiles = {
    kVertSpv,
    kFragSpv,
    kDemoTexture,
    kAltTexture,
    kTexRock,
    kTexGrass,
    kTexMetal,
    kTexWood,
    kHouseModel,
    kMonkeyModel,
    kKenneyTreeDetailed,
    kKenneyTreeSimple,
    kKenneyRockLarge,
    kKenneyCampfire,
    kKenneyTent,
    kKenneyStump,
};
}  // namespace UtilDemoAssets

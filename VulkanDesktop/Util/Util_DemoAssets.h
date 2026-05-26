#pragma once

#include <array>
#include <string_view>

// Repo-relative paths for the current demo scene (Vk_Core). Temporary until scene-load (Docs/scene-load_Plan.md).
// Keep in sync with UtilStartupChecks::VerifyRequiredAssets.
namespace UtilDemoAssets {
inline constexpr std::string_view kVertSpv       = "VulkanDesktop/Shader_Generated/TriangleVert.spv";
inline constexpr std::string_view kFragSpv       = "VulkanDesktop/Shader_Generated/TrianglePix.spv";
inline constexpr std::string_view kDemoTexture   = "Data/Textures/viking_room.png";
inline constexpr std::string_view kAltTexture    = "Data/Textures/RedMoon.jpg";
inline constexpr std::string_view kHouseModel    = "Data/Models/viking_room.obj";
inline constexpr std::string_view kMonkeyModel   = "Data/Models/monkey_smooth.obj";

inline constexpr std::array< std::string_view, 6 > kRequiredFiles = {
    kVertSpv,
    kFragSpv,
    kDemoTexture,
    kAltTexture,
    kHouseModel,
    kMonkeyModel,
};
}  // namespace UtilDemoAssets

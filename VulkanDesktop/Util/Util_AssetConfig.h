#pragma once

#include <filesystem>
#include <string>

// Asset root: single base for repo-relative paths (Data/, VulkanDesktop/Shader_Generated/, …).
// Initialize from Config/engine.json + CLI before UtilLoader::ResolvePath is used.
namespace UtilAssetConfig {
void Initialize( int aArgc, char** aArgv );

std::filesystem::path GetAssetRoot();
std::string         GetConfigPathUsed();
std::string         GetSceneLogicalPath();  // Repo-relative; default kGfxDefaultSceneLogicalPath; overridable via --scene.

void PrintUsage( const char* aProgramName );
}  // namespace UtilAssetConfig

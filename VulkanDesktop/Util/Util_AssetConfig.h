#pragma once

#include <filesystem>
#include <string>

// Asset root accessors (delegate to Util_EngineConfig / Config/engine.json).
// Prefer UtilEngineConfig::Initialize from Application::InitApp.
namespace UtilAssetConfig {
void Initialize( int aArgc, char** aArgv );

std::filesystem::path GetAssetRoot();
std::string         GetConfigPathUsed();
std::string         GetSceneLogicalPath();  // Repo-relative; default kGfxDefaultSceneLogicalPath; overridable via --scene.

void PrintUsage( const char* aProgramName );
}  // namespace UtilAssetConfig

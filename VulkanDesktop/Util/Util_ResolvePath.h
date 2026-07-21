#pragma once

#include <filesystem>
#include <string>

struct Util_EngineConfig;

// Repo-relative path resolution under asset root (no Vulkan / texture I/O).
namespace UtilResolvePath {
std::string Resolve( const std::filesystem::path& aAssetRoot, const std::string& aFilename );
std::string Resolve( const Util_EngineConfig& aConfig, const std::string& aFilename );
}  // namespace UtilResolvePath

#pragma once

#include <string>
#include <vector>

struct Util_EngineConfig;

// File I/O only. Relative paths resolve under Util_EngineConfig::GetAssetRoot(). GPU upload lives in Vk_TextureLoader.
namespace UtilLoader {
std::string         ResolvePath( const Util_EngineConfig& aConfig, const std::string& aFilename );
std::vector< char > ReadFile( const Util_EngineConfig& aConfig, const std::string& aFilename );
}  // namespace UtilLoader

#pragma once

#include <string>

// Repo-relative path resolution under asset root (no Vulkan / texture I/O).
namespace UtilResolvePath {
std::string Resolve( const std::string& aFilename );
}  // namespace UtilResolvePath

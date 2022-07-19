#pragma once
#include <string>
#include <vector>

namespace UtilLoader {
std::vector< char > ReadFile( const std::string& aFilename );

bool LoadTexture();
}  // namespace UtilLoader
#pragma once
#include "../RenderCore/Vk_Types.h"

namespace UtilLoader {
std::string ResolvePath( const std::string& aFilename );
std::vector< char > ReadFile( const std::string& aFilename );

bool LoadTexture(const std::string& aFilename, Texture& aTexture, uint32_t& aTextureMipLevel);
}  // namespace UtilLoader
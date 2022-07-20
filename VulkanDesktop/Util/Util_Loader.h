#pragma once
#include "../RenderCore/Vk_Types.h"

namespace UtilLoader {
std::vector< char > ReadFile( const std::string& aFilename );

bool LoadTexture(const std::string& aFilename, Texture& aTexture, uint32_t& aTextureMipLevel);
}  // namespace UtilLoader
#pragma once
#include "../RenderCore/Vk_Types.h"

// File I/O and image upload. Paths are resolved from cwd / common build output dirs (see ResolvePath).
namespace UtilLoader {
std::string ResolvePath( const std::string& aFilename );
std::vector< char > ReadFile( const std::string& aFilename );

bool LoadTexture( const std::string& aFilename, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel );
}  // namespace UtilLoader
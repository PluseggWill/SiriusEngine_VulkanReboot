#pragma once
#include "../RenderCore/Vk_Types.h"
struct Vk_ResourceContext;

// File I/O and image upload. Relative paths resolve under UtilAssetConfig::GetAssetRoot() (see ResolvePath).
namespace UtilLoader {
std::string ResolvePath( const std::string& aFilename );
std::vector< char > ReadFile( const std::string& aFilename );

bool LoadTexture( const std::string& aFilename, const Vk_ResourceContext& aContext, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel );
}  // namespace UtilLoader
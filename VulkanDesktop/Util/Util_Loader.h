#pragma once
#include "../RenderCore/Vk_Types.h"
struct Vk_ResourceContext;
struct Util_EngineConfig;

// File I/O and image upload. Relative paths resolve under Util_EngineConfig::GetAssetRoot().
namespace UtilLoader {
std::string         ResolvePath( const Util_EngineConfig& aConfig, const std::string& aFilename );
std::vector< char > ReadFile( const Util_EngineConfig& aConfig, const std::string& aFilename );

bool LoadTexture( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel );
}  // namespace UtilLoader
#pragma once
#include "../RenderCore/Vk_Types.h"
struct Vk_ResourceContext;
struct Util_EngineConfig;

// File I/O and image upload. Relative paths resolve under Util_EngineConfig::GetAssetRoot().
namespace UtilLoader {
std::string         ResolvePath( const Util_EngineConfig& aConfig, const std::string& aFilename );
std::vector< char > ReadFile( const Util_EngineConfig& aConfig, const std::string& aFilename );

bool LoadTexture( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel );

// Cubemap faces: posx, negx, posy, negy, posz, negz PNGs in aDirectory (logical path).
bool LoadCubemapFromFaceDirectory( const Util_EngineConfig& aConfig, const std::string& aDirectory, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                   Gfx_Texture& aTextureOut, uint32_t aMipLevels );

// 2D image without sRGB (BRDF LUT).
bool LoadImage2D( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, VkFormat aFormat, Gfx_Texture& aTextureOut );
}  // namespace UtilLoader
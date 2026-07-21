#pragma once

#include "Vk_Types.h"

struct Vk_ResourceContext;
struct Util_EngineConfig;

// GPU texture / cubemap upload (stbi decode + VMA). Path resolve via UtilLoader; no Util↔Vulkan cycle on Util headers.
namespace Vk_TextureLoader {

bool LoadTexture( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, Vk_TextureResource& aTextureOut,
                  uint32_t& aTextureMipLevel );

// Cubemap faces: posx, negx, posy, negy, posz, negz PNGs in aDirectory (logical path).
bool LoadCubemapFromFaceDirectory( const Util_EngineConfig& aConfig, const std::string& aDirectory, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                   Vk_TextureResource& aTextureOut, uint32_t aMipLevels );

// Offline GGX prefilter mips: aRootDir/mip00..mipNN/{face}.png (no GPU box-filter mips).
bool LoadCubemapMipChainFromFaceDirectories( const Util_EngineConfig& aConfig, const std::string& aRootDir, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                             Vk_TextureResource& aTextureOut, uint32_t aMipCount );

// 2D image without sRGB (BRDF LUT).
bool LoadImage2D( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, VkFormat aFormat, Vk_TextureResource& aTextureOut );

}  // namespace Vk_TextureLoader

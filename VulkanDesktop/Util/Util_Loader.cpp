#include "Util_Loader.h"
#include "../RenderCore/Vk_ResourceContext.h"
#include "Util_AssetConfig.h"
#include "Util_EngineConfig.h"
#include "Util_Logger.h"

#include <stb_image.h>

#include <filesystem>
#include <fstream>
#include <vector>

std::string UtilLoader::ResolvePath( const std::string& aFilename ) {
    const std::filesystem::path inputPath( aFilename );

    if ( inputPath.is_absolute() && std::filesystem::exists( inputPath ) ) {
        return std::filesystem::weakly_canonical( inputPath ).string();
    }

    const auto assetRelative = ( UtilAssetConfig::GetAssetRoot() / inputPath ).lexically_normal();
    if ( std::filesystem::exists( assetRelative ) ) {
        return std::filesystem::weakly_canonical( assetRelative ).string();
    }

    if ( std::filesystem::exists( inputPath ) ) {
        return std::filesystem::weakly_canonical( inputPath ).string();
    }

    return assetRelative.string();
}

std::vector< char > UtilLoader::ReadFile( const std::string& aFilename ) {
    const std::string resolvedPath = ResolvePath( aFilename );
    UtilLogger::Debug( "LOADER", "Reading file: " + resolvedPath );
    std::ifstream file( resolvedPath, std::ios::ate | std::ios::binary );

    if ( !file.is_open() ) {
        UtilLogger::Error( "LOADER", "Failed to open file: " + aFilename );
        throw std::runtime_error( "failed to open file" );
    }

    size_t              fileSize = ( size_t )file.tellg();
    std::vector< char > buffer( fileSize );

    file.seekg( 0 );
    file.read( buffer.data(), fileSize );

    file.close();
    return buffer;
}

bool UtilLoader::LoadTexture( const std::string& aFilename, const Vk_ResourceContext& aContext, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel ) {
    const std::string resolvedPath = ResolvePath( aFilename );
    UtilLogger::Info( "RESOURCE", "Loading texture from disk: " + resolvedPath );
    int      texWidth    = 0;
    int      texHeight   = 0;
    int      texChannels = 0;
    stbi_uc* pixels      = stbi_load( resolvedPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

    // WARNING: validate before using dimensions (undefined behavior if read size when load failed).
    if ( !pixels || texWidth <= 0 || texHeight <= 0 ) {
        UtilLogger::Error( "RESOURCE", "Failed to decode texture: " + resolvedPath );
        throw std::runtime_error( "failed to load texture image!" );
    }

    const VkDeviceSize imageSize = static_cast< VkDeviceSize >( texWidth ) * static_cast< VkDeviceSize >( texHeight ) * 4;
    const VmaAllocator allocator = aContext.myAllocator;

    const bool useRuntimeMipmap = UtilEngineConfig::GetFeatures().myRuntimeMipmap;
    aTextureMipLevel            = useRuntimeMipmap ? static_cast< uint32_t >( std::floor( std::log2( std::max( texWidth, texHeight ) ) ) ) + 1 : 1;

    Vk_AllocatedBuffer stagingBuffer;

    aContext.CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* data;
    vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
    memcpy( data, pixels, static_cast< size_t >( imageSize ) );
    vmaUnmapMemory( allocator, stagingBuffer.myAllocation );

    // Clean up the pixel array
    stbi_image_free( pixels );

    const VkExtent3D texExtent = { static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ), 1 };

    aContext.CreateImage( texExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, aTextureMipLevel,
                          VK_SAMPLE_COUNT_1_BIT, aTextureOut.AllocImage() );

    aContext.TransitionImageLayout( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aTextureMipLevel );
    aContext.CopyBufferToImage( stagingBuffer.myBuffer, aTextureOut.Image(), static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ) );

    // Final layout: SHADER_READ_ONLY (GenerateMipmaps leaves mips ready when enabled).
    if ( useRuntimeMipmap ) {
        aContext.GenerateMipmaps( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, aTextureMipLevel );
    }
    else {
        aContext.TransitionImageLayout( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        aTextureMipLevel );
    }

    vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );

    return true;
}
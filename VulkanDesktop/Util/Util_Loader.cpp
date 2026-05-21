#include "Util_Loader.h"
#include "Util_Logger.h"
#include "../RenderCore/Vk_Core.h"

#include <stb_image.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

// Search order when a relative asset path is not found in cwd (VS output dir varies).
std::vector< std::filesystem::path > BuildCandidateBases() {
    const auto cwd = std::filesystem::current_path();
    return {
        cwd,
        cwd / "VulkanDesktop",
        cwd.parent_path(),
        cwd.parent_path() / "VulkanDesktop",
        cwd.parent_path().parent_path(),
        cwd.parent_path().parent_path() / "VulkanDesktop",
    };
}
}  // namespace

std::string UtilLoader::ResolvePath( const std::string& aFilename ) {
    const std::filesystem::path inputPath( aFilename );

    if ( std::filesystem::exists( inputPath ) ) {
        return std::filesystem::weakly_canonical( inputPath ).string();
    }

    for ( const auto& base : BuildCandidateBases() ) {
        const auto candidate = ( base / inputPath ).lexically_normal();
        if ( std::filesystem::exists( candidate ) ) {
            return std::filesystem::weakly_canonical( candidate ).string();
        }
    }

    return aFilename;
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

bool UtilLoader::LoadTexture( const std::string& aFilename, Gfx_Texture& aTextureOut, uint32_t& aTextureMipLevel ) {
    const std::string resolvedPath = ResolvePath( aFilename );
    UtilLogger::Info( "RESOURCE", "Loading texture from disk: " + resolvedPath );
    int      texWidth = 0;
    int      texHeight = 0;
    int      texChannels = 0;
    stbi_uc* pixels = stbi_load( resolvedPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

    // WARNING: validate before using dimensions (undefined behavior if read size when load failed).
    if ( !pixels || texWidth <= 0 || texHeight <= 0 ) {
        UtilLogger::Error( "RESOURCE", "Failed to decode texture: " + resolvedPath );
        throw std::runtime_error( "failed to load texture image!" );
    }

    const VkDeviceSize imageSize = static_cast< VkDeviceSize >( texWidth ) * static_cast< VkDeviceSize >( texHeight ) * 4;
    const VmaAllocator allocator = Vk_Core::GetInstance().myAllocator;

    aTextureMipLevel = USE_RUNTIME_MIPMAP ? static_cast< uint32_t >( std::floor( std::log2( std::max( texWidth, texHeight ) ) ) ) + 1 : 1;

    Vk_AllocatedBuffer stagingBuffer;

    Vk_Core::GetInstance().CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* data;
    vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
    memcpy( data, pixels, static_cast< size_t >( imageSize ) );
    vmaUnmapMemory( allocator, stagingBuffer.myAllocation );

    // Clean up the pixel array
    stbi_image_free( pixels );

    const VkExtent3D texExtent = { static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ), 1 };

    Vk_Core::GetInstance().CreateImage( texExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                                        aTextureMipLevel, VK_SAMPLE_COUNT_1_BIT, aTextureOut.AllocImage() );

    Vk_Core::GetInstance().TransitionImageLayout( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  aTextureMipLevel );
    Vk_Core::GetInstance().CopyBufferToImage( stagingBuffer.myBuffer, aTextureOut.Image(), static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ) );

    // Final layout: SHADER_READ_ONLY (GenerateMipmaps leaves mips ready when enabled).
    if ( USE_RUNTIME_MIPMAP ) {
        Vk_Core::GetInstance().GenerateMipmaps( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, aTextureMipLevel );
    }
    else {
        Vk_Core::GetInstance().TransitionImageLayout( aTextureOut.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aTextureMipLevel );
    }

    vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );

    return true;
}
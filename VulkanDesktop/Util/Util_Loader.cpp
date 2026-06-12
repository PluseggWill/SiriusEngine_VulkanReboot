#include "Util_Loader.h"
#include "../RenderCore/Vk_ResourceContext.h"
#include "Util_EngineConfig.h"
#include "Util_Logger.h"
#include "Util_ResolvePath.h"

#include <stb_image.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr std::array< const char*, 6 > kCubemapFaceNames = { "posx.png", "negx.png", "posy.png", "negy.png", "posz.png", "negz.png" };

bool Upload2DImage( const Vk_ResourceContext& aContext, VkFormat aFormat, int aWidth, int aHeight, const stbi_uc* aPixels, uint32_t aMipLevels, Gfx_Texture& aTextureOut ) {
    const VkDeviceSize imageSize = static_cast< VkDeviceSize >( aWidth ) * static_cast< VkDeviceSize >( aHeight ) * 4;
    const VmaAllocator allocator = aContext.myAllocator;

    Vk_AllocatedBuffer stagingBuffer{};
    aContext.CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* data = nullptr;
    vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
    memcpy( data, aPixels, static_cast< size_t >( imageSize ) );
    vmaUnmapMemory( allocator, stagingBuffer.myAllocation );

    const VkExtent3D texExtent = { static_cast< uint32_t >( aWidth ), static_cast< uint32_t >( aHeight ), 1 };
    aContext.CreateImage( texExtent, aFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY, aMipLevels, VK_SAMPLE_COUNT_1_BIT, aTextureOut.AllocImage() );
    aContext.TransitionImageLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aMipLevels );
    aContext.CopyBufferToImage( stagingBuffer.myBuffer, aTextureOut.Image(), static_cast< uint32_t >( aWidth ), static_cast< uint32_t >( aHeight ) );

    if ( aMipLevels > 1 ) {
        aContext.GenerateMipmaps( aTextureOut.Image(), aFormat, aWidth, aHeight, aMipLevels );
    }
    else {
        aContext.TransitionImageLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aMipLevels );
    }

    aTextureOut.ImageView() = aContext.CreateImageView( aTextureOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT, aMipLevels );
    vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
    return true;
}

}  // namespace

std::string UtilLoader::ResolvePath( const Util_EngineConfig& aConfig, const std::string& aFilename ) {
    return UtilResolvePath::Resolve( aConfig, aFilename );
}

std::vector< char > UtilLoader::ReadFile( const Util_EngineConfig& aConfig, const std::string& aFilename ) {
    const std::string resolvedPath = ResolvePath( aConfig, aFilename );
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

bool UtilLoader::LoadTexture( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, Gfx_Texture& aTextureOut,
                              uint32_t& aTextureMipLevel ) {
    const std::string resolvedPath = ResolvePath( aConfig, aFilename );
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

    const bool useRuntimeMipmap = aConfig.GetFeatures().myRuntimeMipmap;
    aTextureMipLevel            = useRuntimeMipmap ? static_cast< uint32_t >( std::floor( std::log2( std::max( texWidth, texHeight ) ) ) ) + 1 : 1;

    const bool uploaded = Upload2DImage( aContext, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, pixels, aTextureMipLevel, aTextureOut );
    stbi_image_free( pixels );
    return uploaded;
}

bool UtilLoader::LoadImage2D( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, VkFormat aFormat,
                              Gfx_Texture& aTextureOut ) {
    const std::string resolvedPath = ResolvePath( aConfig, aFilename );
    UtilLogger::Info( "RESOURCE", "Loading 2D image: " + resolvedPath );
    int      texWidth    = 0;
    int      texHeight   = 0;
    int      texChannels = 0;
    stbi_uc* pixels      = stbi_load( resolvedPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );
    if ( !pixels || texWidth <= 0 || texHeight <= 0 ) {
        UtilLogger::Error( "RESOURCE", "Failed to decode image: " + resolvedPath );
        throw std::runtime_error( "failed to load image" );
    }

    const bool uploaded = Upload2DImage( aContext, aFormat, texWidth, texHeight, pixels, 1, aTextureOut );
    stbi_image_free( pixels );
    return uploaded;
}

bool UtilLoader::LoadCubemapFromFaceDirectory( const Util_EngineConfig& aConfig, const std::string& aDirectory, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                               Gfx_Texture& aTextureOut, uint32_t aMipLevels ) {
    int faceWidth  = 0;
    int faceHeight = 0;

    std::vector< std::vector< stbi_uc > > facePixels;
    facePixels.reserve( kCubemapFaceNames.size() );

    for ( const char* faceName : kCubemapFaceNames ) {
        const std::string resolvedPath = ResolvePath( aConfig, aDirectory + "/" + faceName );
        int               width        = 0;
        int               height       = 0;
        int               channels     = 0;
        stbi_uc*          pixels       = stbi_load( resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha );
        if ( !pixels || width <= 0 || height <= 0 ) {
            UtilLogger::Error( "RESOURCE", "Failed to decode cubemap face: " + resolvedPath );
            throw std::runtime_error( "failed to load cubemap face" );
        }
        if ( faceWidth == 0 ) {
            faceWidth  = width;
            faceHeight = height;
        }
        else if ( width != faceWidth || height != faceHeight ) {
            stbi_image_free( pixels );
            throw std::runtime_error( "cubemap face size mismatch: " + resolvedPath );
        }

        const size_t           byteCount = static_cast< size_t >( width ) * static_cast< size_t >( height ) * 4;
        std::vector< stbi_uc > copy( byteCount );
        memcpy( copy.data(), pixels, byteCount );
        stbi_image_free( pixels );
        facePixels.push_back( std::move( copy ) );
    }

    const VkDeviceSize faceBytes = static_cast< VkDeviceSize >( faceWidth ) * static_cast< VkDeviceSize >( faceHeight ) * 4;
    const VmaAllocator allocator = aContext.myAllocator;

    aContext.CreateCubemapImage( { static_cast< uint32_t >( faceWidth ), static_cast< uint32_t >( faceHeight ) }, aFormat,
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, aMipLevels, aTextureOut.AllocImage() );
    aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aMipLevels );

    for ( uint32_t faceIndex = 0; faceIndex < static_cast< uint32_t >( facePixels.size() ); ++faceIndex ) {
        Vk_AllocatedBuffer stagingBuffer{};
        aContext.CreateBuffer( faceBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );
        void* data = nullptr;
        vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
        memcpy( data, facePixels[ faceIndex ].data(), static_cast< size_t >( faceBytes ) );
        vmaUnmapMemory( allocator, stagingBuffer.myAllocation );
        aContext.CopyBufferToCubemapFace( stagingBuffer.myBuffer, aTextureOut.Image(), static_cast< uint32_t >( faceWidth ), static_cast< uint32_t >( faceHeight ),
                                          faceIndex );
        vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
    }

    aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aMipLevels );
    aTextureOut.ImageView() = aContext.CreateCubemapImageView( aTextureOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT, aMipLevels );
    UtilLogger::Info( "RESOURCE", "Loaded cubemap faces from " + aDirectory + " size=" + std::to_string( faceWidth ) );
    return true;
}

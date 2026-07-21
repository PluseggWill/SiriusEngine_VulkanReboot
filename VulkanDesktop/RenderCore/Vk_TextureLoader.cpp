// Module: Vk_TextureLoader — decode via UtilImageDecode and upload to Vulkan images.
#include "Vk_TextureLoader.h"

#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_ImageDecode.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_ResourceContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::array< const char*, 6 > kCubemapFaceNames = { "posx.png", "negx.png", "posy.png", "negy.png", "posz.png", "negz.png" };

std::string PrefilterMipSubdir( const std::string& aRootDir, uint32_t aMip ) {
    char mipTag[ 8 ];
    snprintf( mipTag, sizeof( mipTag ), "mip%02u", aMip );
    return aRootDir + "/" + mipTag;
}

bool Upload2DImage( const Vk_ResourceContext& aContext, VkFormat aFormat, int aWidth, int aHeight, const uint8_t* aPixels, uint32_t aMipLevels,
                    Vk_TextureResource& aTextureOut ) {
    const VkDeviceSize imageSize = static_cast< VkDeviceSize >( aWidth ) * static_cast< VkDeviceSize >( aHeight ) * 4;
    const VmaAllocator allocator = aContext.myAllocator;

    Vk_AllocatedBuffer stagingBuffer{};
    aContext.CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* data = nullptr;
    vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
    std::memcpy( data, aPixels, static_cast< size_t >( imageSize ) );
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

bool Vk_TextureLoader::LoadTexture( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, Vk_TextureResource& aTextureOut,
                                    uint32_t& aTextureMipLevel ) {
    const std::string      resolvedPath = UtilLoader::ResolvePath( aConfig, aFilename );
    Util_DecodedRgba8Image decoded{};
    UtilLogger::Info( "RESOURCE", "Loading texture from disk: " + resolvedPath );
    if ( !UtilImageDecode::LoadRgba8FromFile( resolvedPath, decoded ) ) {
        throw std::runtime_error( "failed to load texture image!" );
    }

    const bool useRuntimeMipmap = aConfig.GetFeatures().myRuntimeMipmap;
    aTextureMipLevel            = useRuntimeMipmap ? static_cast< uint32_t >( std::floor( std::log2( std::max( decoded.myWidth, decoded.myHeight ) ) ) ) + 1 : 1;

    return Upload2DImage( aContext, VK_FORMAT_R8G8B8A8_SRGB, decoded.myWidth, decoded.myHeight, decoded.myPixels.data(), aTextureMipLevel, aTextureOut );
}

bool Vk_TextureLoader::LoadImage2D( const Util_EngineConfig& aConfig, const std::string& aFilename, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                    Vk_TextureResource& aTextureOut ) {
    const std::string      resolvedPath = UtilLoader::ResolvePath( aConfig, aFilename );
    Util_DecodedRgba8Image decoded{};
    UtilLogger::Info( "RESOURCE", "Loading 2D image: " + resolvedPath );
    if ( !UtilImageDecode::LoadRgba8FromFile( resolvedPath, decoded ) ) {
        throw std::runtime_error( "failed to load image" );
    }

    return Upload2DImage( aContext, aFormat, decoded.myWidth, decoded.myHeight, decoded.myPixels.data(), 1, aTextureOut );
}

bool Vk_TextureLoader::LoadCubemapFromFaceDirectory( const Util_EngineConfig& aConfig, const std::string& aDirectory, const Vk_ResourceContext& aContext, VkFormat aFormat,
                                                     Vk_TextureResource& aTextureOut, uint32_t aMipLevels ) {
    int faceWidth  = 0;
    int faceHeight = 0;

    std::vector< std::vector< uint8_t > > facePixels;
    facePixels.reserve( kCubemapFaceNames.size() );

    for ( const char* faceName : kCubemapFaceNames ) {
        const std::string      resolvedPath = UtilLoader::ResolvePath( aConfig, aDirectory + "/" + faceName );
        Util_DecodedRgba8Image decoded{};
        if ( !UtilImageDecode::LoadRgba8FromFile( resolvedPath, decoded ) ) {
            throw std::runtime_error( "failed to load cubemap face" );
        }
        if ( faceWidth == 0 ) {
            faceWidth  = decoded.myWidth;
            faceHeight = decoded.myHeight;
        }
        else if ( decoded.myWidth != faceWidth || decoded.myHeight != faceHeight ) {
            throw std::runtime_error( "cubemap face size mismatch: " + resolvedPath );
        }

        facePixels.push_back( std::move( decoded.myPixels ) );
    }

    const VkDeviceSize faceBytes = static_cast< VkDeviceSize >( faceWidth ) * static_cast< VkDeviceSize >( faceHeight ) * 4;
    const VmaAllocator allocator = aContext.myAllocator;

    const VkImageUsageFlags cubemapUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    aContext.CreateCubemapImage( { static_cast< uint32_t >( faceWidth ), static_cast< uint32_t >( faceHeight ) }, aFormat, cubemapUsage, VMA_MEMORY_USAGE_GPU_ONLY, aMipLevels,
                                 aTextureOut.AllocImage() );
    aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aMipLevels );

    for ( uint32_t faceIndex = 0; faceIndex < static_cast< uint32_t >( facePixels.size() ); ++faceIndex ) {
        Vk_AllocatedBuffer stagingBuffer{};
        aContext.CreateBuffer( faceBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );
        void* data = nullptr;
        vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
        std::memcpy( data, facePixels[ faceIndex ].data(), static_cast< size_t >( faceBytes ) );
        vmaUnmapMemory( allocator, stagingBuffer.myAllocation );
        aContext.CopyBufferToCubemapFace( stagingBuffer.myBuffer, aTextureOut.Image(), static_cast< uint32_t >( faceWidth ), static_cast< uint32_t >( faceHeight ),
                                          faceIndex );
        vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
    }

    if ( aMipLevels > 1 ) {
        aContext.GenerateCubemapMipmaps( aTextureOut.Image(), aFormat, faceWidth, faceHeight, aMipLevels );
    }
    else {
        aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aMipLevels );
    }

    aTextureOut.ImageView() = aContext.CreateCubemapImageView( aTextureOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT, aMipLevels );
    UtilLogger::Info( "RESOURCE", "Loaded cubemap faces from " + aDirectory + " size=" + std::to_string( faceWidth ) );
    return true;
}

bool Vk_TextureLoader::LoadCubemapMipChainFromFaceDirectories( const Util_EngineConfig& aConfig, const std::string& aRootDir, const Vk_ResourceContext& aContext,
                                                               VkFormat aFormat, Vk_TextureResource& aTextureOut, uint32_t aMipCount ) {
    if ( aMipCount == 0u ) {
        throw std::runtime_error( "LoadCubemapMipChainFromFaceDirectories: mipCount must be >= 1" );
    }

    int baseFaceWidth  = 0;
    int baseFaceHeight = 0;
    for ( const char* faceName : kCubemapFaceNames ) {
        const std::string      resolvedPath = UtilLoader::ResolvePath( aConfig, PrefilterMipSubdir( aRootDir, 0 ) + "/" + faceName );
        Util_DecodedRgba8Image decoded{};
        if ( !UtilImageDecode::LoadRgba8FromFile( resolvedPath, decoded ) ) {
            throw std::runtime_error( "failed to load prefilter mip chain mip00" );
        }
        if ( baseFaceWidth == 0 ) {
            baseFaceWidth  = decoded.myWidth;
            baseFaceHeight = decoded.myHeight;
        }
        else if ( decoded.myWidth != baseFaceWidth || decoded.myHeight != baseFaceHeight ) {
            throw std::runtime_error( "prefilter mip0 face size mismatch: " + resolvedPath );
        }
    }

    const VkImageUsageFlags cubemapUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    aContext.CreateCubemapImage( { static_cast< uint32_t >( baseFaceWidth ), static_cast< uint32_t >( baseFaceHeight ) }, aFormat, cubemapUsage, VMA_MEMORY_USAGE_GPU_ONLY,
                                 aMipCount, aTextureOut.AllocImage() );
    aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aMipCount );

    const VmaAllocator allocator = aContext.myAllocator;
    for ( uint32_t mip = 0; mip < aMipCount; ++mip ) {
        const uint32_t    faceWidth  = std::max( 1u, static_cast< uint32_t >( baseFaceWidth ) >> mip );
        const uint32_t    faceHeight = std::max( 1u, static_cast< uint32_t >( baseFaceHeight ) >> mip );
        const std::string mipDir     = PrefilterMipSubdir( aRootDir, mip );

        for ( uint32_t faceIndex = 0; faceIndex < static_cast< uint32_t >( kCubemapFaceNames.size() ); ++faceIndex ) {
            const std::string      resolvedPath = UtilLoader::ResolvePath( aConfig, mipDir + "/" + kCubemapFaceNames[ faceIndex ] );
            Util_DecodedRgba8Image decoded{};
            if ( !UtilImageDecode::LoadRgba8FromFile( resolvedPath, decoded ) ) {
                throw std::runtime_error( "failed to load prefilter mip chain face" );
            }
            if ( static_cast< uint32_t >( decoded.myWidth ) != faceWidth || static_cast< uint32_t >( decoded.myHeight ) != faceHeight ) {
                throw std::runtime_error( "prefilter mip face size mismatch: " + resolvedPath );
            }

            const VkDeviceSize faceBytes = static_cast< VkDeviceSize >( faceWidth ) * static_cast< VkDeviceSize >( faceHeight ) * 4;
            Vk_AllocatedBuffer stagingBuffer{};
            aContext.CreateBuffer( faceBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );
            void* data = nullptr;
            vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
            std::memcpy( data, decoded.myPixels.data(), static_cast< size_t >( faceBytes ) );
            vmaUnmapMemory( allocator, stagingBuffer.myAllocation );

            aContext.CopyBufferToCubemapFace( stagingBuffer.myBuffer, aTextureOut.Image(), faceWidth, faceHeight, faceIndex, mip );
            vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );
        }
    }

    aContext.TransitionCubemapLayout( aTextureOut.Image(), aFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, aMipCount );
    aTextureOut.ImageView() = aContext.CreateCubemapImageView( aTextureOut.Image(), aFormat, VK_IMAGE_ASPECT_COLOR_BIT, aMipCount );
    UtilLogger::Info( "RESOURCE",
                      "Loaded offline prefilter mip chain from " + aRootDir + " mips=" + std::to_string( aMipCount ) + " base=" + std::to_string( baseFaceWidth ) );
    return true;
}

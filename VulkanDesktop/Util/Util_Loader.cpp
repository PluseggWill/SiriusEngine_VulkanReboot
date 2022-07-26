#include "Util_Loader.h"
#include "../RenderCore/Vk_Core.h"

#include <stb_image.h>

#include <fstream>

std::vector< char > UtilLoader::ReadFile( const std::string& aFilename ) {
    std::ifstream file( aFilename, std::ios::ate | std::ios::binary );

    if ( !file.is_open() ) {
        throw std::runtime_error( "failed to open file" );
    }

    size_t              fileSize = ( size_t )file.tellg();
    std::vector< char > buffer( fileSize );

    file.seekg( 0 );
    file.read( buffer.data(), fileSize );

    file.close();
    return buffer;
}

bool UtilLoader::LoadTexture(const std::string& aFilename, Texture& aTexture, uint32_t& aTextureMipLevel) {
    int          texWidth, texHeight, texChannels;
    stbi_uc*     pixels     = stbi_load( aFilename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );
    VkDeviceSize imageSize  = texWidth * texHeight * 4;
    VmaAllocator allocator  = Vk_Core::GetInstance().myAllocator;

    aTextureMipLevel = USE_RUNTIME_MIPMAP ? static_cast< uint32_t >( std::floor( std::log2( std::max( texWidth, texHeight ) ) ) ) + 1 : 1;

    if ( !pixels ) {
        throw std::runtime_error( "failed to load texture image!" );
    }

    AllocatedBuffer stagingBuffer;

    Vk_Core::GetInstance().CreateBuffer( imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* data;
    vmaMapMemory( allocator, stagingBuffer.myAllocation, &data );
    memcpy( data, pixels, static_cast< size_t >( imageSize ) );
    vmaUnmapMemory( allocator, stagingBuffer.myAllocation );

    // Clean up the pixel array
    stbi_image_free( pixels );

    VkExtent3D texExtent = { static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ), 1 };

    Vk_Core::GetInstance().CreateImage( texExtent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
                                        aTextureMipLevel, VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );

    // Transition for copy buffer to image
    Vk_Core::GetInstance().TransitionImageLayout( aTexture.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  aTextureMipLevel );
    Vk_Core::GetInstance().CopyBufferToImage( stagingBuffer.myBuffer, aTexture.Image(), static_cast< uint32_t >( texWidth ), static_cast< uint32_t >( texHeight ) );

    // Transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
    if ( USE_RUNTIME_MIPMAP ) {
        Vk_Core::GetInstance().GenerateMipmaps( aTexture.Image(), VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, aTextureMipLevel );
    }
    else {
        Vk_Core::GetInstance().TransitionImageLayout( aTexture.Image(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aTextureMipLevel );
    }

    vmaDestroyBuffer( allocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );

    return true;
}
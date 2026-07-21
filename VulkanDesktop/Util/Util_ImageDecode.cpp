#include "Util_ImageDecode.h"

#include "Util_Logger.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>

#include <cstring>

bool UtilImageDecode::LoadRgba8FromFile( const std::string& aResolvedPath, Util_DecodedRgba8Image& aOut ) {
    aOut = {};

    int      width    = 0;
    int      height   = 0;
    int      channels = 0;
    stbi_uc* pixels   = stbi_load( aResolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha );
    if ( !pixels || width <= 0 || height <= 0 ) {
        if ( pixels != nullptr ) {
            stbi_image_free( pixels );
        }
        UtilLogger::Error( "IMAGE", "Failed to decode RGBA8 image: " + aResolvedPath );
        return false;
    }

    const size_t byteCount = static_cast< size_t >( width ) * static_cast< size_t >( height ) * 4u;
    aOut.myPixels.resize( byteCount );
    std::memcpy( aOut.myPixels.data(), pixels, byteCount );
    stbi_image_free( pixels );

    aOut.myWidth  = width;
    aOut.myHeight = height;
    return true;
}

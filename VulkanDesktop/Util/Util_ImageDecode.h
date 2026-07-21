#pragma once

#include <cstdint>
#include <string>
#include <vector>

// CPU RGBA8 decode (stb_image). Callers never include stb or call stbi_* directly.
struct Util_DecodedRgba8Image {
    std::vector< uint8_t > myPixels;
    int                    myWidth  = 0;
    int                    myHeight = 0;

    bool IsValid() const {
        return !myPixels.empty() && myWidth > 0 && myHeight > 0;
    }
};

namespace UtilImageDecode {

// Loads forced RGBA8 from a resolved filesystem path. Returns false on decode failure (aOut cleared).
bool LoadRgba8FromFile( const std::string& aResolvedPath, Util_DecodedRgba8Image& aOut );

}  // namespace UtilImageDecode

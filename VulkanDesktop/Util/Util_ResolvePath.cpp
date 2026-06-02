#include "Util_ResolvePath.h"

#include "Util_AssetConfig.h"

#include <filesystem>

namespace UtilResolvePath {

std::string Resolve( const std::string& aFilename ) {
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

}  // namespace UtilResolvePath

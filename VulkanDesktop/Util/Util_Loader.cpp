#include "Util_Loader.h"

#include "Util_EngineConfig.h"
#include "Util_Logger.h"
#include "Util_ResolvePath.h"

#include <fstream>
#include <stdexcept>
#include <vector>

std::string UtilLoader::ResolvePath( const std::filesystem::path& aAssetRoot, const std::string& aFilename ) {
    return UtilResolvePath::Resolve( aAssetRoot, aFilename );
}

std::string UtilLoader::ResolvePath( const Util_EngineConfig& aConfig, const std::string& aFilename ) {
    return ResolvePath( aConfig.GetAssetRoot(), aFilename );
}

std::vector< char > UtilLoader::ReadFile( const std::filesystem::path& aAssetRoot, const std::string& aFilename ) {
    const std::string resolvedPath = ResolvePath( aAssetRoot, aFilename );
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

std::vector< char > UtilLoader::ReadFile( const Util_EngineConfig& aConfig, const std::string& aFilename ) {
    return ReadFile( aConfig.GetAssetRoot(), aFilename );
}

#include "Util_AssetConfig.h"

#include "Util_EngineConfig.h"

namespace UtilAssetConfig {

void PrintUsage( const char* aProgramName ) {
    UtilEngineConfig::PrintUsage( aProgramName );
}

void Initialize( int aArgc, char** aArgv ) {
    UtilEngineConfig::Initialize( aArgc, aArgv );
}

std::string GetSceneLogicalPath() {
    return UtilEngineConfig::GetSceneLogicalPath();
}

std::filesystem::path GetAssetRoot() {
    return UtilEngineConfig::GetAssetRoot();
}

std::string GetConfigPathUsed() {
    return UtilEngineConfig::GetConfigPathUsed();
}

}  // namespace UtilAssetConfig

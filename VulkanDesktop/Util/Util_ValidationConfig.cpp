#include "Util_ValidationConfig.h"

#include "Util_EngineConfig.h"

namespace UtilValidationConfig {

void ParseCli( int aArgc, char** aArgv ) {
    (void)aArgc;
    (void)aArgv;
}

void LoadFromConfigFile( const std::string& aConfigPath ) {
    (void)aConfigPath;
}

bool ResolveEnabled( bool aBuildDefault ) {
    return UtilEngineConfig::ResolveValidationEnabled( aBuildDefault );
}

bool IsEnabled() {
    return UtilEngineConfig::IsValidationEnabled();
}

const std::vector< const char* >& GetRequestedLayerNames() {
    return UtilEngineConfig::GetValidationLayerNames();
}

}  // namespace UtilValidationConfig

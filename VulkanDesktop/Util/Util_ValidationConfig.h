#pragma once

#include <optional>
#include <string>
#include <vector>

// Validation layer enablement: CLI > Config/engine.json > build default (Debug on, Release off).
namespace UtilValidationConfig {
constexpr const char* kDefaultLayerName = "VK_LAYER_KHRONOS_validation";

void ParseCli( int aArgc, char** aArgv );
void LoadFromConfigFile( const std::string& aConfigPath );
bool  ResolveEnabled( bool aBuildDefault );
bool  IsEnabled();

const std::vector< const char* >& GetRequestedLayerNames();
}  // namespace UtilValidationConfig

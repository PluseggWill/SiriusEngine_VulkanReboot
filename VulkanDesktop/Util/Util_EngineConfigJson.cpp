#include "Util_EngineConfig.h"

#include "Util_AssetManifest.h"
#include "Util_EngineConfigInternal.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

using UtilEngineConfigInternal::ParseLogLevel;
using UtilEngineConfigInternal::ResolvePathArgument;

void Util_EngineConfig::ApplyJsonFile( const std::filesystem::path& aConfigPath ) {
    std::ifstream file( aConfigPath );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Could not open config file: " + aConfigPath.string() );
    }

    nlohmann::json root;
    try {
        file >> root;
    }
    catch ( const nlohmann::json::exception& e ) {
        throw std::runtime_error( std::string( "Invalid JSON in config: " ) + aConfigPath.string() + " — " + e.what() );
    }

    if ( root.contains( "assetRoot" ) && root[ "assetRoot" ].is_string() ) {
        const std::string assetRoot = root[ "assetRoot" ].get< std::string >();
        if ( !assetRoot.empty() ) {
            myAssetRoot = ResolvePathArgument( assetRoot );
        }
    }

    if ( root.contains( "scene" ) && root[ "scene" ].is_string() ) {
        mySceneLogicalPath = root[ "scene" ].get< std::string >();
    }

    if ( root.contains( "window" ) && root[ "window" ].is_object() ) {
        const auto& window = root[ "window" ];
        if ( window.contains( "width" ) && window[ "width" ].is_number_unsigned() ) {
            myWindowWidth = window[ "width" ].get< uint32_t >();
        }
        if ( window.contains( "height" ) && window[ "height" ].is_number_unsigned() ) {
            myWindowHeight = window[ "height" ].get< uint32_t >();
        }
    }

    if ( root.contains( "vsync" ) && root[ "vsync" ].is_boolean() ) {
        myVsync = root[ "vsync" ].get< bool >();
    }

    if ( root.contains( "logLevel" ) && root[ "logLevel" ].is_string() ) {
        myMinLogLevel = ParseLogLevel( root[ "logLevel" ].get< std::string >() );
    }

    if ( root.contains( "logFile" ) && root[ "logFile" ].is_string() ) {
        myLogFilePath = root[ "logFile" ].get< std::string >();
    }

    if ( root.contains( "enableValidationLayers" ) && root[ "enableValidationLayers" ].is_boolean() ) {
        myConfigValidation = root[ "enableValidationLayers" ].get< bool >();
    }

    if ( root.contains( "features" ) && root[ "features" ].is_object() ) {
        const auto& features = root[ "features" ];
        if ( features.contains( "demoRotate" ) && features[ "demoRotate" ].is_boolean() ) {
            myFeatures.myDemoRotate = features[ "demoRotate" ].get< bool >();
        }
        if ( features.contains( "lodEnabled" ) && features[ "lodEnabled" ].is_boolean() ) {
            myFeatures.myLodEnabled = features[ "lodEnabled" ].get< bool >();
        }
        if ( features.contains( "runtimeMipmap" ) && features[ "runtimeMipmap" ].is_boolean() ) {
            myFeatures.myRuntimeMipmap = features[ "runtimeMipmap" ].get< bool >();
        }
    }

    if ( root.contains( "assetVerify" ) && root[ "assetVerify" ].is_string() ) {
        auto toLower = []( std::string aValue ) {
            for ( char& c : aValue ) {
                c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
            }
            return aValue;
        };
        const std::string policy = toLower( root[ "assetVerify" ].get< std::string >() );
        if ( policy == "strict" ) {
            myAssetVerifyPolicy = Util_AssetVerifyPolicy::Strict;
        }
        else if ( policy == "warn" ) {
            myAssetVerifyPolicy = Util_AssetVerifyPolicy::Warn;
        }
        else {
            throw std::runtime_error( "Invalid assetVerify in config (expected strict|warn): " + policy );
        }
    }

    if ( root.contains( "shaderPermutation" ) && root[ "shaderPermutation" ].is_string() ) {
        myConfigShaderPermutation = root[ "shaderPermutation" ].get< std::string >();
    }
    if ( root.contains( "renderPreset" ) && root[ "renderPreset" ].is_string() ) {
        myConfigRenderPreset = root[ "renderPreset" ].get< std::string >();
    }

    if ( root.contains( "lighting" ) && root[ "lighting" ].is_object() ) {
        const auto& lighting = root[ "lighting" ];
        if ( lighting.contains( "shadowsEnabled" ) && lighting[ "shadowsEnabled" ].is_boolean() ) {
            myLightingSettings.myShadowsEnabled = lighting[ "shadowsEnabled" ].get< bool >();
        }
        if ( lighting.contains( "iblEnabled" ) && lighting[ "iblEnabled" ].is_boolean() ) {
            myLightingSettings.myIblEnabled = lighting[ "iblEnabled" ].get< bool >();
        }
        if ( lighting.contains( "iblIntensity" ) && lighting[ "iblIntensity" ].is_number() ) {
            myLightingSettings.myIblIntensity = lighting[ "iblIntensity" ].get< float >();
        }
        if ( lighting.contains( "environment" ) && lighting[ "environment" ].is_string() ) {
            myEnvironmentPath = lighting[ "environment" ].get< std::string >();
        }
        if ( lighting.contains( "ddgiEnabled" ) && lighting[ "ddgiEnabled" ].is_boolean() ) {
            myLightingSettings.myDdgiEnabled = lighting[ "ddgiEnabled" ].get< bool >();
        }
        if ( lighting.contains( "ddgiIntensity" ) && lighting[ "ddgiIntensity" ].is_number() ) {
            myLightingSettings.myDdgiIntensity = lighting[ "ddgiIntensity" ].get< float >();
        }
        if ( lighting.contains( "ddgiStaggeredUpdate" ) && lighting[ "ddgiStaggeredUpdate" ].is_boolean() ) {
            myLightingSettings.myDdgiStaggeredUpdate = lighting[ "ddgiStaggeredUpdate" ].get< bool >();
        }
        if ( lighting.contains( "ddgiUpdateBudget" ) && lighting[ "ddgiUpdateBudget" ].is_number_unsigned() ) {
            myLightingSettings.myDdgiUpdateBudget = lighting[ "ddgiUpdateBudget" ].get< uint32_t >();
        }
        if ( lighting.contains( "ddgiHistoryBlend" ) && lighting[ "ddgiHistoryBlend" ].is_number() ) {
            myLightingSettings.myDdgiHistoryBlend = lighting[ "ddgiHistoryBlend" ].get< float >();
        }
        if ( lighting.contains( "ddgiVolumeCenter" ) && lighting[ "ddgiVolumeCenter" ].is_array() && lighting[ "ddgiVolumeCenter" ].size() == 3 ) {
            myLightingSettings.myDdgiVolumeCenter = glm::vec3( lighting[ "ddgiVolumeCenter" ][ 0 ].get< float >(), lighting[ "ddgiVolumeCenter" ][ 1 ].get< float >(),
                                                               lighting[ "ddgiVolumeCenter" ][ 2 ].get< float >() );
        }
        if ( lighting.contains( "ddgiVolumeExtents" ) && lighting[ "ddgiVolumeExtents" ].is_array() && lighting[ "ddgiVolumeExtents" ].size() == 3 ) {
            myLightingSettings.myDdgiVolumeExtents = glm::vec3( lighting[ "ddgiVolumeExtents" ][ 0 ].get< float >(), lighting[ "ddgiVolumeExtents" ][ 1 ].get< float >(),
                                                                lighting[ "ddgiVolumeExtents" ][ 2 ].get< float >() );
        }
    }
}

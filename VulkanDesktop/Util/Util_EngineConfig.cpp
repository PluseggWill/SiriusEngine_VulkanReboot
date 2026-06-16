#include "Util_EngineConfig.h"

#include "../Gfx/Gfx_RenderPreset.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "Util_AssetManifest.h"
#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

UtilLogger::LogLevel ParseLogLevel( const std::string& aValue ) {
    auto toLower = []( std::string aValue ) {
        for ( char& c : aValue ) {
            c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
        }
        return aValue;
    };
    const std::string v = toLower( aValue );
    if ( v == "debug" ) {
        return UtilLogger::LogLevel::Debug;
    }
    if ( v == "info" ) {
        return UtilLogger::LogLevel::Info;
    }
    if ( v == "warn" || v == "warning" ) {
        return UtilLogger::LogLevel::Warning;
    }
    if ( v == "error" ) {
        return UtilLogger::LogLevel::Error;
    }
    throw std::runtime_error( "Invalid logLevel in config (expected debug|info|warn|error): " + aValue );
}

std::filesystem::path FindRepoRoot() {
    std::filesystem::path dir = std::filesystem::current_path();
    for ( int i = 0; i < 10; ++i ) {
        if ( std::filesystem::exists( dir / "VulkanDesktop.sln" ) ) {
            return dir;
        }
        if ( std::filesystem::exists( dir / "VulkanDesktop" / "VulkanDesktop.vcxproj" ) ) {
            return dir;
        }
        if ( !dir.has_parent_path() || dir == dir.parent_path() ) {
            break;
        }
        dir = dir.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path ResolvePathArgument( const std::string& aPath ) {
    const std::filesystem::path input( aPath );
    if ( input.is_absolute() ) {
        return std::filesystem::weakly_canonical( input );
    }
    return std::filesystem::weakly_canonical( std::filesystem::current_path() / input );
}

std::filesystem::path DefaultConfigPath() {
    return FindRepoRoot() / "Config" / "engine.json";
}

void RequireDirectory( const std::filesystem::path& aPath, const char* aLabel ) {
    if ( !std::filesystem::exists( aPath ) || !std::filesystem::is_directory( aPath ) ) {
        throw std::runtime_error( std::string( aLabel ) + " is not a directory: " + aPath.string() );
    }
}

const char* LogLevelName( UtilLogger::LogLevel aLevel ) {
    switch ( aLevel ) {
    case UtilLogger::LogLevel::Debug:
        return "debug";
    case UtilLogger::LogLevel::Info:
        return "info";
    case UtilLogger::LogLevel::Warning:
        return "warn";
    case UtilLogger::LogLevel::Error:
        return "error";
    default:
        return "unknown";
    }
}

}  // namespace

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
    }
}

void Util_EngineConfig::ResolveActiveShaderPermutation( const CliOverrides& aOverrides ) {
    if ( aOverrides.myShaderPermutation.has_value() ) {
        myShaderPermutationName = *aOverrides.myShaderPermutation;
        return;
    }
    if ( aOverrides.myRenderPreset.has_value() ) {
        myRenderPresetName      = *aOverrides.myRenderPreset;
        myShaderPermutationName = Gfx_RenderPreset::ToShaderPermutationName( myRenderPresetName );
        return;
    }
    if ( myConfigShaderPermutation.has_value() ) {
        myShaderPermutationName = *myConfigShaderPermutation;
        return;
    }
    if ( myConfigRenderPreset.has_value() ) {
        myRenderPresetName      = *myConfigRenderPreset;
        myShaderPermutationName = Gfx_RenderPreset::ToShaderPermutationName( myRenderPresetName );
        return;
    }
    // Default when config omits renderPreset (HybridDeferred maps to lit permutation).
    myRenderPresetName = "HybridDeferred";
}

Util_EngineConfig::CliOverrides Util_EngineConfig::ParseCliOverrides( int aArgc, char** aArgv ) {
    CliOverrides overrides;
    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--asset-root" || arg == "--assetroot" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --asset-root" );
            }
            overrides.myAssetRoot = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--config" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --config" );
            }
            overrides.myConfigPath = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--scene" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --scene" );
            }
            overrides.myScene = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--log-level" || arg == "--loglevel" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --log-level" );
            }
            overrides.myLogLevel = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--vsync" ) {
            overrides.myVsync = true;
            continue;
        }
        if ( arg == "--no-vsync" ) {
            overrides.myVsync = false;
            continue;
        }
        if ( arg == "--width" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --width" );
            }
            overrides.myWindowWidth = static_cast< uint32_t >( std::stoul( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--height" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --height" );
            }
            overrides.myWindowHeight = static_cast< uint32_t >( std::stoul( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--demo-rotate" ) {
            overrides.myDemoRotate = true;
            continue;
        }
        if ( arg == "--no-demo-rotate" ) {
            overrides.myDemoRotate = false;
            continue;
        }
        if ( arg == "--lod-enabled" ) {
            overrides.myLodEnabled = true;
            continue;
        }
        if ( arg == "--no-lod-enabled" ) {
            overrides.myLodEnabled = false;
            continue;
        }
        if ( arg == "--legacy-direct-draw" ) {
            overrides.myLegacyDirectDraw = true;
            continue;
        }
        if ( arg == "--gpu-cull" ) {
            overrides.myGpuCullEnabled = true;
            continue;
        }
        if ( arg == "--no-gpu-cull" ) {
            overrides.myGpuCullEnabled = false;
            continue;
        }
        if ( arg == "--smoke-frames" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --smoke-frames" );
            }
            overrides.mySmokeFrames = std::stoi( aArgv[ ++i ] );
            if ( *overrides.mySmokeFrames <= 0 ) {
                throw std::runtime_error( "--smoke-frames must be > 0" );
            }
            continue;
        }
        if ( arg == "--smoke-seconds" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --smoke-seconds" );
            }
            overrides.mySmokeSeconds = std::stod( aArgv[ ++i ] );
            if ( *overrides.mySmokeSeconds <= 0.0 ) {
                throw std::runtime_error( "--smoke-seconds must be > 0" );
            }
            continue;
        }
        if ( arg == "--validation" || arg == "--enable-validation" ) {
            myCliValidationOverride = true;
            continue;
        }
        if ( arg == "--no-validation" || arg == "--disable-validation" ) {
            myCliValidationOverride = false;
            continue;
        }
        if ( arg == "--shader-permutation" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --shader-permutation" );
            }
            overrides.myShaderPermutation = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--render-preset" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --render-preset" );
            }
            overrides.myRenderPreset = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--descriptor-layout-mismatch-test" ) {
            myDescriptorLayoutMismatchTest = true;
            continue;
        }
        if ( arg == "--renderdoc" ) {
            myEnableRenderDoc = true;
            continue;
        }
        if ( arg == "--perf-log" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --perf-log" );
            }
            overrides.myPerfLog = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--shadows" ) {
            overrides.myShadowsEnabled = true;
            continue;
        }
        if ( arg == "--no-shadows" ) {
            overrides.myShadowsEnabled = false;
            continue;
        }
        if ( arg == "--ibl" ) {
            overrides.myIblEnabled = true;
            continue;
        }
        if ( arg == "--no-ibl" ) {
            overrides.myIblEnabled = false;
            continue;
        }
        if ( arg == "--ibl-intensity" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --ibl-intensity" );
            }
            overrides.myIblIntensity = static_cast< float >( std::stod( aArgv[ ++i ] ) );
            continue;
        }
        if ( arg == "--environment" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --environment" );
            }
            overrides.myEnvironment = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--ddgi" ) {
            overrides.myDdgiEnabled = true;
            continue;
        }
        if ( arg == "--no-ddgi" ) {
            overrides.myDdgiEnabled = false;
            continue;
        }
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            continue;
        }
        throw std::runtime_error( "Unknown argument: " + arg );
    }
    return overrides;
}

void Util_EngineConfig::ApplyCliOverrides( const CliOverrides& aOverrides ) {
    if ( aOverrides.myAssetRoot.has_value() ) {
        myAssetRoot = ResolvePathArgument( *aOverrides.myAssetRoot );
    }
    if ( aOverrides.myScene.has_value() ) {
        mySceneLogicalPath = *aOverrides.myScene;
    }
    if ( aOverrides.myLogLevel.has_value() ) {
        myMinLogLevel = ParseLogLevel( *aOverrides.myLogLevel );
    }
    if ( aOverrides.myVsync.has_value() ) {
        myVsync = *aOverrides.myVsync;
    }
    if ( aOverrides.myWindowWidth.has_value() ) {
        myWindowWidth = *aOverrides.myWindowWidth;
    }
    if ( aOverrides.myWindowHeight.has_value() ) {
        myWindowHeight = *aOverrides.myWindowHeight;
    }
    if ( aOverrides.myDemoRotate.has_value() ) {
        myFeatures.myDemoRotate = *aOverrides.myDemoRotate;
    }
    if ( aOverrides.myLodEnabled.has_value() ) {
        myFeatures.myLodEnabled = *aOverrides.myLodEnabled;
    }
    if ( aOverrides.myLegacyDirectDraw.has_value() ) {
        myLegacyDirectDraw = *aOverrides.myLegacyDirectDraw;
    }
    if ( aOverrides.myGpuCullEnabled.has_value() ) {
        myGpuCullEnabled = *aOverrides.myGpuCullEnabled;
    }
    if ( aOverrides.myRuntimeMipmap.has_value() ) {
        myFeatures.myRuntimeMipmap = *aOverrides.myRuntimeMipmap;
    }
    if ( aOverrides.mySmokeFrames.has_value() ) {
        mySmokeFrameLimit = *aOverrides.mySmokeFrames;
    }
    if ( aOverrides.mySmokeSeconds.has_value() ) {
        mySmokeSeconds = *aOverrides.mySmokeSeconds;
    }
    if ( aOverrides.myPerfLog.has_value() ) {
        myPerfLogPath = *aOverrides.myPerfLog;
    }
    if ( aOverrides.myShadowsEnabled.has_value() ) {
        myLightingSettings.myShadowsEnabled = *aOverrides.myShadowsEnabled;
    }
    if ( aOverrides.myIblEnabled.has_value() ) {
        myLightingSettings.myIblEnabled = *aOverrides.myIblEnabled;
    }
    if ( aOverrides.myIblIntensity.has_value() ) {
        myLightingSettings.myIblIntensity = *aOverrides.myIblIntensity;
    }
    if ( aOverrides.myEnvironment.has_value() ) {
        myEnvironmentPath = *aOverrides.myEnvironment;
    }
    if ( aOverrides.myDdgiEnabled.has_value() ) {
        myLightingSettings.myDdgiEnabled = *aOverrides.myDdgiEnabled;
    }
    ResolveActiveShaderPermutation( aOverrides );
}

void Util_EngineConfig::LoadFromArgv( int aArgc, char** aArgv ) {
    mySceneLogicalPath = kGfxDefaultSceneLogicalPath;
    myAssetRoot        = FindRepoRoot();

    CliOverrides overrides = ParseCliOverrides( aArgc, aArgv );

    const std::filesystem::path configPath = overrides.myConfigPath ? ResolvePathArgument( overrides.myConfigPath->string() ) : DefaultConfigPath();
    myConfigPathUsed                       = configPath.string();

    if ( std::filesystem::exists( configPath ) ) {
        ApplyJsonFile( configPath );
    }
    else if ( overrides.myConfigPath.has_value() ) {
        throw std::runtime_error( "Config file not found: " + myConfigPathUsed );
    }

    ApplyCliOverrides( overrides );

    if ( !overrides.myAssetRoot.has_value() && myAssetRoot.empty() ) {
        myAssetRoot = FindRepoRoot();
    }

    RequireDirectory( myAssetRoot, "asset root" );

    if ( myWindowWidth == 0 || myWindowHeight == 0 ) {
        throw std::runtime_error( "window width/height must be > 0" );
    }
}

std::filesystem::path Util_EngineConfig::GetAssetRoot() const {
    return myAssetRoot;
}

std::string Util_EngineConfig::GetConfigPathUsed() const {
    return myConfigPathUsed;
}

std::string Util_EngineConfig::GetSceneLogicalPath() const {
    return mySceneLogicalPath;
}

std::string Util_EngineConfig::GetLogFilePath() const {
    return myLogFilePath;
}

std::string Util_EngineConfig::GetPerfLogPath() const {
    return myPerfLogPath;
}

uint32_t Util_EngineConfig::GetWindowWidth() const {
    return myWindowWidth;
}

uint32_t Util_EngineConfig::GetWindowHeight() const {
    return myWindowHeight;
}

bool Util_EngineConfig::GetVsync() const {
    return myVsync;
}

UtilLogger::LogLevel Util_EngineConfig::GetMinLogLevel() const {
    return myMinLogLevel;
}

const Util_EngineConfig::FeatureFlags& Util_EngineConfig::GetFeatures() const {
    return myFeatures;
}

Util_AssetVerifyPolicy Util_EngineConfig::GetAssetVerifyPolicy() const {
    return myAssetVerifyPolicy;
}

int Util_EngineConfig::GetSmokeFrameLimit() const {
    return mySmokeFrameLimit;
}

double Util_EngineConfig::GetSmokeSeconds() const {
    return mySmokeSeconds;
}

bool Util_EngineConfig::GetDescriptorLayoutMismatchTest() const {
    return myDescriptorLayoutMismatchTest;
}

bool Util_EngineConfig::GetEnableRenderDoc() const {
    return myEnableRenderDoc;
}

bool Util_EngineConfig::GetLegacyDirectDraw() const {
    return myLegacyDirectDraw;
}

bool Util_EngineConfig::GetGpuCullEnabled() const {
    return myGpuCullEnabled;
}

bool Util_EngineConfig::ResolveValidationEnabled( bool aBuildDefault ) const {
    if ( myCliValidationOverride.has_value() ) {
        myValidationEnabled = *myCliValidationOverride;
    }
    else if ( myConfigValidation.has_value() ) {
        myValidationEnabled = *myConfigValidation;
    }
    else {
        myValidationEnabled = aBuildDefault;
    }

    myValidationResolved = true;
    return myValidationEnabled;
}

bool Util_EngineConfig::IsValidationEnabled() const {
    if ( !myValidationResolved ) {
        return ResolveValidationEnabled( false );
    }
    return myValidationEnabled;
}

const std::vector< const char* >& Util_EngineConfig::GetValidationLayerNames() const {
    return myValidationLayers;
}

const std::string& Util_EngineConfig::GetShaderPermutationName() const {
    return myShaderPermutationName;
}

const std::string& Util_EngineConfig::GetRenderPresetName() const {
    return myRenderPresetName;
}

const Gfx_LightingSettings& Util_EngineConfig::GetLightingSettings() const {
    return myLightingSettings;
}

const std::string& Util_EngineConfig::GetEnvironmentPath() const {
    return myEnvironmentPath;
}

void Util_EngineConfig::LogResolvedSummary() const {
    UtilLogger::Info( "CONFIG", "cwd=" + std::filesystem::current_path().string() );
    UtilLogger::Info( "CONFIG", "config=" + myConfigPathUsed );
    UtilLogger::Info( "CONFIG", "assetRoot=" + std::filesystem::weakly_canonical( myAssetRoot ).string() );
    UtilLogger::Info( "CONFIG", "scene=" + mySceneLogicalPath );
    UtilLogger::Info( "CONFIG", "window=" + std::to_string( myWindowWidth ) + "x" + std::to_string( myWindowHeight ) + " vsync=" + ( myVsync ? "on" : "off" ) );
    UtilLogger::Info( "CONFIG", std::string( "logLevel=" ) + LogLevelName( myMinLogLevel ) );
    UtilLogger::Info( "CONFIG", std::string( "features demoRotate=" ) + ( myFeatures.myDemoRotate ? "true" : "false" ) + " lodEnabled="
                                    + ( myFeatures.myLodEnabled ? "true" : "false" ) + " runtimeMipmap=" + ( myFeatures.myRuntimeMipmap ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "assetVerify=" ) + ( myAssetVerifyPolicy == Util_AssetVerifyPolicy::Strict ? "strict" : "warn" ) );
    if ( !myRenderPresetName.empty() ) {
        UtilLogger::Info( "CONFIG", "renderPreset=" + myRenderPresetName );
    }
    UtilLogger::Info( "CONFIG", "shaderPermutation=" + myShaderPermutationName );
    UtilLogger::Info( "CONFIG", std::string( "renderdoc=" ) + ( myEnableRenderDoc ? "enabled" : "disabled" ) );
    UtilLogger::Info( "CONFIG", std::string( "legacyDirectDraw=" ) + ( myLegacyDirectDraw ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "gpuCull=" ) + ( myGpuCullEnabled ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "lighting shadows=" ) + ( myLightingSettings.myShadowsEnabled ? "1" : "0" )
                                    + " ibl=" + ( myLightingSettings.myIblEnabled ? "1" : "0" ) + " iblIntensity=" + std::to_string( myLightingSettings.myIblIntensity )
                                    + " ddgi=" + ( myLightingSettings.myDdgiEnabled ? "1" : "0" ) + " environment=" + myEnvironmentPath );

    if ( myValidationResolved ) {
        const char* source = "build default";
        if ( myCliValidationOverride.has_value() ) {
            source = "CLI";
        }
        else if ( myConfigValidation.has_value() ) {
            source = "config";
        }
        UtilLogger::Info( "CONFIG", std::string( "validationLayers=" ) + ( myValidationEnabled ? "enabled" : "disabled" ) + " (" + source + ")" );
    }
}

namespace UtilEngineConfig {

bool TryEarlyExitFromCli( int aArgc, char** aArgv ) {
    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            PrintUsage( aArgc > 0 ? aArgv[ 0 ] : nullptr );
            return true;
        }
    }
    return false;
}

void PrintUsage( const char* aProgramName ) {
    const char* name = ( aProgramName != nullptr && aProgramName[ 0 ] != '\0' ) ? aProgramName : "VulkanDesktop";
    std::cerr << "Usage: " << name << " [options]\n"
              << "  --config <file>        JSON config (default Config/engine.json under repo root)\n"
              << "  --asset-root <dir>     Repository / content root (contains Data/, VulkanDesktop/)\n"
              << "  --scene <path>         Scene JSON (repo-relative; default Data/Scenes/sponza.json)\n"
              << "  --width <n>            Window width (overrides config)\n"
              << "  --height <n>           Window height (overrides config)\n"
              << "  --vsync / --no-vsync   Swapchain present mode preference\n"
              << "  --log-level <level>    debug | info | warn | error\n"
              << "  --validation / --enable-validation   Enable Vulkan validation layers\n"
              << "  --no-validation / --disable-validation   Disable validation layers\n"
              << "  --demo-rotate / --no-demo-rotate   Demo Z spin on entities\n"
              << "  --lod-enabled / --no-lod-enabled   CPU mesh LOD selection in draw stream\n"
              << "  --legacy-direct-draw   Use vkCmdDrawIndexed per draw (M2 prep debug fallback)\n"
              << "  --gpu-cull / --no-gpu-cull   Enable compute frustum cull to slot indirect buffer (P3; default off)\n"
              << "  --smoke-frames <n>     Exit after n rendered frames (dev smoke / CI)\n"
              << "  --smoke-seconds <s>    Exit after s seconds in main loop (post scene load; task smoke)\n"
              << "  --perf-log <path>      Append per-frame JSONL metrics (schemaVersion 1)\n"
              << "  --shader-permutation <name>   Active entry in PermutationRegistry.json (e.g. lit, lit_alpha_clip)\n"
              << "  --render-preset <name>        Render preset (HybridDeferred, ForwardLit, ForwardLitAlphaClip); overridden by --shader-permutation\n"
              << "  --shadows / --no-shadows      Enable or disable directional shadow map (overrides config)\n"
              << "  --ibl / --no-ibl              Enable or disable split-sum IBL (overrides config)\n"
              << "  --ibl-intensity <f>           IBL intensity multiplier (overrides config)\n"
              << "  --ddgi / --no-ddgi            Enable or disable DDGI probe contribution\n"
              << "  --environment <path>          IBL cubemap manifest directory (repo-relative)\n"
              << "  --descriptor-layout-mismatch-test   Dev: vkUpdateDescriptorSets type mismatch probe (needs --validation)\n"
              << "  --renderdoc            Enable RenderDoc runtime integration (startup-gated)\n"
              << "  --help                 Show this message\n"
              << "\nFull reference: Docs/CLI.md (engine.json keys, priority, examples).\n";
}

}  // namespace UtilEngineConfig

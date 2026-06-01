#include "Util_EngineConfig.h"

#include "../Gfx/Gfx_SceneDesc.h"
#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

constexpr const char* kDefaultValidationLayer = "VK_LAYER_KHRONOS_validation";

bool                     gInitialized = false;
std::filesystem::path    gAssetRoot;
std::string              gConfigPathUsed;
std::string              gSceneLogicalPath = kGfxDefaultSceneLogicalPath;
std::string              gLogFilePath;
uint32_t                 gWindowWidth  = 1600;
uint32_t                 gWindowHeight = 1200;
bool                     gVsync        = true;
UtilLogger::LogLevel     gMinLogLevel  = UtilLogger::LogLevel::Info;
UtilEngineConfig::FeatureFlags gFeatures{};
Util_AssetVerifyPolicy         gAssetVerifyPolicy = Util_AssetVerifyPolicy::Strict;
int                            gSmokeFrameLimit   = 0;
double                         gSmokeSeconds      = 0.0;
bool                           gDescriptorLayoutMismatchTest = false;
std::optional< bool >    gCliValidationOverride;
std::optional< bool >    gConfigValidation;
bool                     gValidationResolved = false;
bool                     gValidationEnabled   = false;
std::vector< const char* > gValidationLayers = { kDefaultValidationLayer };

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

std::string ToLower( std::string aValue ) {
    for ( char& c : aValue ) {
        c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
    }
    return aValue;
}

UtilLogger::LogLevel ParseLogLevel( const std::string& aValue ) {
    const std::string v = ToLower( aValue );
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

std::filesystem::path DefaultConfigPath() {
    return FindRepoRoot() / "Config" / "engine.json";
}

void RequireDirectory( const std::filesystem::path& aPath, const char* aLabel ) {
    if ( !std::filesystem::exists( aPath ) || !std::filesystem::is_directory( aPath ) ) {
        throw std::runtime_error( std::string( aLabel ) + " is not a directory: " + aPath.string() );
    }
}

void ApplyJsonFile( const std::filesystem::path& aConfigPath ) {
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
            gAssetRoot = ResolvePathArgument( assetRoot );
        }
    }

    if ( root.contains( "scene" ) && root[ "scene" ].is_string() ) {
        gSceneLogicalPath = root[ "scene" ].get< std::string >();
    }

    if ( root.contains( "window" ) && root[ "window" ].is_object() ) {
        const auto& window = root[ "window" ];
        if ( window.contains( "width" ) && window[ "width" ].is_number_unsigned() ) {
            gWindowWidth = window[ "width" ].get< uint32_t >();
        }
        if ( window.contains( "height" ) && window[ "height" ].is_number_unsigned() ) {
            gWindowHeight = window[ "height" ].get< uint32_t >();
        }
    }

    if ( root.contains( "vsync" ) && root[ "vsync" ].is_boolean() ) {
        gVsync = root[ "vsync" ].get< bool >();
    }

    if ( root.contains( "logLevel" ) && root[ "logLevel" ].is_string() ) {
        gMinLogLevel = ParseLogLevel( root[ "logLevel" ].get< std::string >() );
    }

    if ( root.contains( "logFile" ) && root[ "logFile" ].is_string() ) {
        gLogFilePath = root[ "logFile" ].get< std::string >();
    }

    if ( root.contains( "enableValidationLayers" ) && root[ "enableValidationLayers" ].is_boolean() ) {
        gConfigValidation = root[ "enableValidationLayers" ].get< bool >();
    }

    if ( root.contains( "features" ) && root[ "features" ].is_object() ) {
        const auto& features = root[ "features" ];
        if ( features.contains( "demoRotate" ) && features[ "demoRotate" ].is_boolean() ) {
            gFeatures.myDemoRotate = features[ "demoRotate" ].get< bool >();
        }
        if ( features.contains( "runtimeMipmap" ) && features[ "runtimeMipmap" ].is_boolean() ) {
            gFeatures.myRuntimeMipmap = features[ "runtimeMipmap" ].get< bool >();
        }
    }

    if ( root.contains( "assetVerify" ) && root[ "assetVerify" ].is_string() ) {
        const std::string policy = ToLower( root[ "assetVerify" ].get< std::string >() );
        if ( policy == "strict" ) {
            gAssetVerifyPolicy = Util_AssetVerifyPolicy::Strict;
        }
        else if ( policy == "warn" ) {
            gAssetVerifyPolicy = Util_AssetVerifyPolicy::Warn;
        }
        else {
            throw std::runtime_error( "Invalid assetVerify in config (expected strict|warn): " + policy );
        }
    }
}

struct CliOverrides {
    std::optional< std::string >             myAssetRoot;
    std::optional< std::filesystem::path >   myConfigPath;
    std::optional< std::string >             myScene;
    std::optional< std::string >             myLogLevel;
    std::optional< bool >                    myVsync;
    std::optional< uint32_t >                myWindowWidth;
    std::optional< uint32_t >                myWindowHeight;
    std::optional< bool >                    myDemoRotate;
    std::optional< bool >                    myRuntimeMipmap;
    std::optional< int >                     mySmokeFrames;
    std::optional< double >                  mySmokeSeconds;
};

CliOverrides ParseCliOverrides( int aArgc, char** aArgv ) {
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
            gCliValidationOverride = true;
            continue;
        }
        if ( arg == "--no-validation" || arg == "--disable-validation" ) {
            gCliValidationOverride = false;
            continue;
        }
        if ( arg == "--descriptor-layout-mismatch-test" ) {
            gDescriptorLayoutMismatchTest = true;
            continue;
        }
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            continue;
        }
        throw std::runtime_error( "Unknown argument: " + arg );
    }
    return overrides;
}

void ApplyCliOverrides( const CliOverrides& aOverrides ) {
    if ( aOverrides.myAssetRoot.has_value() ) {
        gAssetRoot = ResolvePathArgument( *aOverrides.myAssetRoot );
    }
    if ( aOverrides.myScene.has_value() ) {
        gSceneLogicalPath = *aOverrides.myScene;
    }
    if ( aOverrides.myLogLevel.has_value() ) {
        gMinLogLevel = ParseLogLevel( *aOverrides.myLogLevel );
    }
    if ( aOverrides.myVsync.has_value() ) {
        gVsync = *aOverrides.myVsync;
    }
    if ( aOverrides.myWindowWidth.has_value() ) {
        gWindowWidth = *aOverrides.myWindowWidth;
    }
    if ( aOverrides.myWindowHeight.has_value() ) {
        gWindowHeight = *aOverrides.myWindowHeight;
    }
    if ( aOverrides.myDemoRotate.has_value() ) {
        gFeatures.myDemoRotate = *aOverrides.myDemoRotate;
    }
    if ( aOverrides.myRuntimeMipmap.has_value() ) {
        gFeatures.myRuntimeMipmap = *aOverrides.myRuntimeMipmap;
    }
    if ( aOverrides.mySmokeFrames.has_value() ) {
        gSmokeFrameLimit = *aOverrides.mySmokeFrames;
    }
    if ( aOverrides.mySmokeSeconds.has_value() ) {
        gSmokeSeconds = *aOverrides.mySmokeSeconds;
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
              << "  --scene <path>         Scene JSON (repo-relative; default Data/Scenes/demo.json)\n"
              << "  --width <n>            Window width (overrides config)\n"
              << "  --height <n>           Window height (overrides config)\n"
              << "  --vsync / --no-vsync   Swapchain present mode preference\n"
              << "  --log-level <level>    debug | info | warn | error\n"
              << "  --validation / --enable-validation   Enable Vulkan validation layers\n"
              << "  --no-validation / --disable-validation   Disable validation layers\n"
              << "  --demo-rotate / --no-demo-rotate   Demo Z spin on entities\n"
              << "  --smoke-frames <n>     Exit after n rendered frames (dev smoke / CI)\n"
              << "  --smoke-seconds <s>    Exit after s seconds in main loop (post scene load; task smoke)\n"
              << "  --descriptor-layout-mismatch-test   Dev: vkUpdateDescriptorSets type mismatch probe (needs --validation)\n"
              << "  --help                 Show this message\n"
              << "\nFull reference: Docs/CLI.md (engine.json keys, priority, examples).\n";
}

void Initialize( int aArgc, char** aArgv ) {
    if ( gInitialized ) {
        return;
    }

    gAssetRoot = FindRepoRoot();

    CliOverrides overrides = ParseCliOverrides( aArgc, aArgv );

    const std::filesystem::path configPath =
        overrides.myConfigPath ? ResolvePathArgument( overrides.myConfigPath->string() ) : DefaultConfigPath();
    gConfigPathUsed = configPath.string();

    if ( std::filesystem::exists( configPath ) ) {
        ApplyJsonFile( configPath );
    }
    else if ( overrides.myConfigPath.has_value() ) {
        throw std::runtime_error( "Config file not found: " + gConfigPathUsed );
    }

    ApplyCliOverrides( overrides );

    if ( !overrides.myAssetRoot.has_value() && gAssetRoot.empty() ) {
        gAssetRoot = FindRepoRoot();
    }

    RequireDirectory( gAssetRoot, "asset root" );

    if ( gWindowWidth == 0 || gWindowHeight == 0 ) {
        throw std::runtime_error( "window width/height must be > 0" );
    }

    gInitialized = true;
}

bool IsInitialized() {
    return gInitialized;
}

std::filesystem::path GetAssetRoot() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gAssetRoot;
}

std::string GetConfigPathUsed() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gConfigPathUsed;
}

std::string GetSceneLogicalPath() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gSceneLogicalPath;
}

std::string GetLogFilePath() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gLogFilePath;
}

uint32_t GetWindowWidth() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gWindowWidth;
}

uint32_t GetWindowHeight() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gWindowHeight;
}

bool GetVsync() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gVsync;
}

UtilLogger::LogLevel GetMinLogLevel() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gMinLogLevel;
}

const FeatureFlags& GetFeatures() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gFeatures;
}

Util_AssetVerifyPolicy GetAssetVerifyPolicy() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gAssetVerifyPolicy;
}

int GetSmokeFrameLimit() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gSmokeFrameLimit;
}

double GetSmokeSeconds() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gSmokeSeconds;
}

bool GetDescriptorLayoutMismatchTest() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gDescriptorLayoutMismatchTest;
}

bool ResolveValidationEnabled( bool aBuildDefault ) {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }

    if ( gCliValidationOverride.has_value() ) {
        gValidationEnabled = *gCliValidationOverride;
    }
    else if ( gConfigValidation.has_value() ) {
        gValidationEnabled = *gConfigValidation;
    }
    else {
        gValidationEnabled = aBuildDefault;
    }

    gValidationResolved = true;
    return gValidationEnabled;
}

bool IsValidationEnabled() {
    if ( !gValidationResolved ) {
        return ResolveValidationEnabled( false );
    }
    return gValidationEnabled;
}

const std::vector< const char* >& GetValidationLayerNames() {
    return gValidationLayers;
}

void LogResolvedSummary() {
    if ( !gInitialized ) {
        return;
    }

    UtilLogger::Info( "CONFIG", "cwd=" + std::filesystem::current_path().string() );
    UtilLogger::Info( "CONFIG", "config=" + gConfigPathUsed );
    UtilLogger::Info( "CONFIG", "assetRoot=" + std::filesystem::weakly_canonical( gAssetRoot ).string() );
    UtilLogger::Info( "CONFIG", "scene=" + gSceneLogicalPath );
    UtilLogger::Info( "CONFIG",
                      "window=" + std::to_string( gWindowWidth ) + "x" + std::to_string( gWindowHeight ) + " vsync=" +
                          ( gVsync ? "on" : "off" ) );
    UtilLogger::Info( "CONFIG", std::string( "logLevel=" ) + LogLevelName( gMinLogLevel ) );
    UtilLogger::Info( "CONFIG",
                      std::string( "features demoRotate=" ) + ( gFeatures.myDemoRotate ? "true" : "false" ) + " runtimeMipmap=" +
                          ( gFeatures.myRuntimeMipmap ? "true" : "false" ) );
    UtilLogger::Info( "CONFIG", std::string( "assetVerify=" ) + ( gAssetVerifyPolicy == Util_AssetVerifyPolicy::Strict ? "strict" : "warn" ) );

    if ( gValidationResolved ) {
        const char* source = "build default";
        if ( gCliValidationOverride.has_value() ) {
            source = "CLI";
        }
        else if ( gConfigValidation.has_value() ) {
            source = "config";
        }
        UtilLogger::Info( "CONFIG",
                          std::string( "validationLayers=" ) + ( gValidationEnabled ? "enabled" : "disabled" ) + " (" + source + ")" );
    }
}

}  // namespace UtilEngineConfig

#include "Util_AssetConfig.h"
#include "Util_Logger.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {

std::filesystem::path gAssetRoot;
std::string         gConfigPathUsed;
bool                gInitialized = false;

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

std::string Trim( std::string aValue ) {
    auto isSpace = []( unsigned char c ) { return std::isspace( c ) != 0; };
    while ( !aValue.empty() && isSpace( static_cast< unsigned char >( aValue.front() ) ) ) {
        aValue.erase( aValue.begin() );
    }
    while ( !aValue.empty() && isSpace( static_cast< unsigned char >( aValue.back() ) ) ) {
        aValue.pop_back();
    }
    return aValue;
}

std::optional< std::string > ParseAssetRootFromJson( const std::string& aContent ) {
    const std::string key = "\"assetRoot\"";
    const auto        keyPos = aContent.find( key );
    if ( keyPos == std::string::npos ) {
        return std::nullopt;
    }

    const auto colonPos = aContent.find( ':', keyPos + key.size() );
    if ( colonPos == std::string::npos ) {
        return std::nullopt;
    }

    const auto openQuote = aContent.find( '"', colonPos + 1 );
    if ( openQuote == std::string::npos ) {
        return std::nullopt;
    }

    std::string value;
    for ( size_t i = openQuote + 1; i < aContent.size(); ++i ) {
        const char c = aContent[ i ];
        if ( c == '"' ) {
            return Trim( value );
        }
        if ( c == '\\' && i + 1 < aContent.size() ) {
            value.push_back( aContent[ ++i ] );
            continue;
        }
        value.push_back( c );
    }

    return std::nullopt;
}

std::filesystem::path DefaultConfigPath() {
    return FindRepoRoot() / "Config" / "engine.json";
}

std::optional< std::string > LoadAssetRootFromConfigFile( const std::filesystem::path& aConfigPath ) {
    std::ifstream file( aConfigPath );
    if ( !file.is_open() ) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return ParseAssetRootFromJson( buffer.str() );
}

void RequireDirectory( const std::filesystem::path& aPath, const char* aLabel ) {
    if ( !std::filesystem::exists( aPath ) || !std::filesystem::is_directory( aPath ) ) {
        throw std::runtime_error( std::string( aLabel ) + " is not a directory: " + aPath.string() );
    }
}

}  // namespace

void UtilAssetConfig::PrintUsage( const char* aProgramName ) {
    const char* name = ( aProgramName != nullptr && aProgramName[ 0 ] != '\0' ) ? aProgramName : "VulkanDesktop";
    std::cerr << "Usage: " << name << " [options]\n"
              << "  --asset-root <dir>   Repository / content root (contains Data/, VulkanDesktop/)\n"
              << "  --config <file>      JSON config (assetRoot, enableValidationLayers)\n"
              << "  --validation         Enable Vulkan validation layers\n"
              << "  --no-validation      Disable Vulkan validation layers\n"
              << "  --help               Show this message\n";
}

void UtilAssetConfig::Initialize( int aArgc, char** aArgv ) {
    if ( gInitialized ) {
        return;
    }

    std::optional< std::string > cliAssetRoot;
    std::optional< std::filesystem::path > cliConfigPath;

    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--help" || arg == "-h" || arg == "/?" ) {
            PrintUsage( aArgc > 0 ? aArgv[ 0 ] : nullptr );
            std::exit( 0 );
        }
        if ( arg == "--asset-root" || arg == "--assetroot" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --asset-root" );
            }
            cliAssetRoot = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--config" ) {
            if ( i + 1 >= aArgc ) {
                throw std::runtime_error( "Missing value for --config" );
            }
            cliConfigPath = aArgv[ ++i ];
            continue;
        }
        if ( arg == "--validation" || arg == "--enable-validation" || arg == "--no-validation" || arg == "--disable-validation" ) {
            continue;
        }
        throw std::runtime_error( "Unknown argument: " + arg );
    }

    const std::filesystem::path configPath = cliConfigPath ? ResolvePathArgument( cliConfigPath->string() ) : DefaultConfigPath();
    gConfigPathUsed                          = configPath.string();

    std::optional< std::string > configAssetRoot;
    if ( std::filesystem::exists( configPath ) ) {
        configAssetRoot = LoadAssetRootFromConfigFile( configPath );
        if ( !configAssetRoot.has_value() ) {
            UtilLogger::Warn( "CONFIG", "Could not parse assetRoot from: " + gConfigPathUsed );
        }
    }
    else if ( cliConfigPath.has_value() ) {
        UtilLogger::Warn( "CONFIG", "Config file not found: " + gConfigPathUsed );
    }

    if ( cliAssetRoot.has_value() ) {
        gAssetRoot = ResolvePathArgument( *cliAssetRoot );
    }
    else if ( configAssetRoot.has_value() && !configAssetRoot->empty() ) {
        gAssetRoot = ResolvePathArgument( *configAssetRoot );
    }
    else {
        gAssetRoot = FindRepoRoot();
    }

    RequireDirectory( gAssetRoot, "asset root" );

    gInitialized = true;
    UtilLogger::Info( "CONFIG", "cwd=" + std::filesystem::current_path().string() );
    UtilLogger::Info( "CONFIG", "config=" + gConfigPathUsed );
    UtilLogger::Info( "CONFIG", "assetRoot=" + std::filesystem::weakly_canonical( gAssetRoot ).string() );
}

std::filesystem::path UtilAssetConfig::GetAssetRoot() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gAssetRoot;
}

std::string UtilAssetConfig::GetConfigPathUsed() {
    if ( !gInitialized ) {
        Initialize( 0, nullptr );
    }
    return gConfigPathUsed;
}

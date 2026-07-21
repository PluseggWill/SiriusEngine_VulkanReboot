#pragma once

// Shared helpers for Util_EngineConfig_* translation units (not a public API).
#include "Util_EngineConfig.h"

#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace UtilEngineConfigInternal {

inline UtilLogger::LogLevel ParseLogLevel( const std::string& aValue ) {
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

inline std::filesystem::path FindRepoRoot() {
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

inline std::filesystem::path ResolvePathArgument( const std::string& aPath ) {
    const std::filesystem::path input( aPath );
    if ( input.is_absolute() ) {
        return std::filesystem::weakly_canonical( input );
    }
    return std::filesystem::weakly_canonical( std::filesystem::current_path() / input );
}

inline std::filesystem::path DefaultConfigPath() {
    return FindRepoRoot() / "Config" / "engine.json";
}

inline void RequireDirectory( const std::filesystem::path& aPath, const char* aLabel ) {
    if ( !std::filesystem::exists( aPath ) || !std::filesystem::is_directory( aPath ) ) {
        throw std::runtime_error( std::string( aLabel ) + " is not a directory: " + aPath.string() );
    }
}

inline const char* LogLevelName( UtilLogger::LogLevel aLevel ) {
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

}  // namespace UtilEngineConfigInternal

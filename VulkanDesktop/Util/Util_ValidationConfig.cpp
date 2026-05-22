#include "Util_ValidationConfig.h"
#include "Util_Logger.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::optional< bool >     gCliOverride;
std::optional< bool >     gConfigValue;
bool                      gResolvedEnabled = false;
bool                      gResolved         = false;
std::vector< const char* > gLayerNames     = { UtilValidationConfig::kDefaultLayerName };

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

std::optional< bool > ParseBoolAfterKey( const std::string& aContent, const std::string& aKey ) {
    const auto keyPos = aContent.find( aKey );
    if ( keyPos == std::string::npos ) {
        return std::nullopt;
    }

    const auto colonPos = aContent.find( ':', keyPos + aKey.size() );
    if ( colonPos == std::string::npos ) {
        return std::nullopt;
    }

    const std::string tail = Trim( aContent.substr( colonPos + 1 ) );
    if ( tail.rfind( "true", 0 ) == 0 ) {
        return true;
    }
    if ( tail.rfind( "false", 0 ) == 0 ) {
        return false;
    }
    return std::nullopt;
}

}  // namespace

void UtilValidationConfig::ParseCli( int aArgc, char** aArgv ) {
    for ( int i = 1; i < aArgc; ++i ) {
        const std::string arg = aArgv[ i ];
        if ( arg == "--validation" || arg == "--enable-validation" ) {
            gCliOverride = true;
            continue;
        }
        if ( arg == "--no-validation" || arg == "--disable-validation" ) {
            gCliOverride = false;
            continue;
        }
    }
}

void UtilValidationConfig::LoadFromConfigFile( const std::string& aConfigPath ) {
    std::ifstream file( aConfigPath );
    if ( !file.is_open() ) {
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    const auto parsed = ParseBoolAfterKey( content, "\"enableValidationLayers\"" );
    if ( parsed.has_value() ) {
        gConfigValue = parsed;
    }
}

bool UtilValidationConfig::ResolveEnabled( bool aBuildDefault ) {
    if ( gCliOverride.has_value() ) {
        gResolvedEnabled = *gCliOverride;
        UtilLogger::Info( "CONFIG", std::string( "validationLayers=" ) + ( gResolvedEnabled ? "enabled (CLI)" : "disabled (CLI)" ) );
    }
    else if ( gConfigValue.has_value() ) {
        gResolvedEnabled = *gConfigValue;
        UtilLogger::Info( "CONFIG", std::string( "validationLayers=" ) + ( gResolvedEnabled ? "enabled (config)" : "disabled (config)" ) );
    }
    else {
        gResolvedEnabled = aBuildDefault;
        UtilLogger::Info( "CONFIG", std::string( "validationLayers=" ) + ( gResolvedEnabled ? "enabled (build default)" : "disabled (build default)" ) );
    }

    gResolved = true;
    return gResolvedEnabled;
}

bool UtilValidationConfig::IsEnabled() {
    return gResolvedEnabled;
}

const std::vector< const char* >& UtilValidationConfig::GetRequestedLayerNames() {
    return gLayerNames;
}

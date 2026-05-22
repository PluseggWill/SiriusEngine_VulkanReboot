#include "Util_StartupChecks.h"
#include "Util_DemoAssets.h"
#include "Util_Loader.h"
#include "Util_Logger.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

void UtilStartupChecks::VerifyRequiredAssets() {
    UtilLogger::Info( "STARTUP", "Verifying required demo assets." );

    std::vector< std::string > missing;
    for ( const std::string_view logical : UtilDemoAssets::kRequiredFiles ) {
        const std::string logicalStr( logical );
        const std::string resolved = UtilLoader::ResolvePath( logicalStr );
        const std::filesystem::path path( resolved );
        if ( !std::filesystem::exists( path ) || !std::filesystem::is_regular_file( path ) ) {
            missing.push_back( logicalStr + " (resolved: " + resolved + ")" );
            UtilLogger::Error( "STARTUP", "Missing required file: " + logicalStr + " (resolved: " + resolved + ")" );
        }
        else {
            UtilLogger::Info( "STARTUP", "OK " + logicalStr + " -> " + resolved );
        }
    }

    if ( !missing.empty() ) {
        std::string message = "Missing " + std::to_string( missing.size() ) + " required asset(s). First: " + missing.front();
        throw std::runtime_error( message );
    }

    UtilLogger::Info( "STARTUP", "All required demo assets present." );
}

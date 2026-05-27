#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "App/Application.h"
#include "Util/Util_Logger.h"
#include "Util/Util_ValidationConfig.h"
#include <cstdlib>
#include <vector>

const uint32_t WIDTH  = 1600;
const uint32_t HEIGHT = 1200;

const std::vector< const char* > deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

int main( int argc, char** argv ) {
    UtilLogger::Init();
    UtilLogger::Info( "APP", "Application startup." );

    UtilValidationConfig::ParseCli( argc, argv );

    Application application;
    application.Configure( WIDTH, HEIGHT, deviceExtensions );

    try {
        const int exitCode = application.Run( argc, argv );
        UtilLogger::Info( "APP", "Application shutdown complete." );
        UtilLogger::Shutdown();
        return exitCode;
    }
    catch ( const std::exception& e ) {
        UtilLogger::Error( "APP", std::string( "Unhandled exception: " ) + e.what() );
        UtilLogger::Shutdown();
        return EXIT_FAILURE;
    }
}

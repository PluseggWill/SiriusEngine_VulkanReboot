#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "App/Application.h"
#include "Util/Util_EngineConfig.h"
#include "Util/Util_Logger.h"
#include <cstdlib>
#include <iostream>
#include <vector>

const std::vector< const char* > deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

int main( int argc, char** argv ) {
    if ( UtilEngineConfig::TryEarlyExitFromCli( argc, argv ) ) {
        return EXIT_SUCCESS;
    }

    Application application;
    application.Configure( deviceExtensions );

    try {
        const int exitCode = application.Run( argc, argv );
        UtilLogger::Shutdown();
        return exitCode;
    }
    catch ( const std::exception& e ) {
        std::cerr << e.what() << std::endl;
        UtilLogger::Shutdown();
        return EXIT_FAILURE;
    }
}

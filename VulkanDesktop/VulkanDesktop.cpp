#define GLFW_INCLUDE_VULKAN
#include <GLFW\glfw3.h>

#include "RenderCore/Vk_Core.h"
#include "Util/Util_AssetConfig.h"
#include "Util/Util_Logger.h"
#include "Util/Util_StartupChecks.h"
#include "Util/Util_ValidationConfig.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

const uint32_t WIDTH  = 1600;
const uint32_t HEIGHT = 1200;

const std::vector< const char* > deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

int main( int argc, char** argv ) {
    UtilLogger::Init();
    UtilLogger::Info( "APP", "Application startup." );

    UtilValidationConfig::ParseCli( argc, argv );

    Vk_Core* app = &Vk_Core::GetInstance();
    app->SetSize( WIDTH, HEIGHT );
    app->SetRequiredExtension( deviceExtensions );

    try {
        UtilAssetConfig::Initialize( argc, argv );
        UtilValidationConfig::LoadFromConfigFile( UtilAssetConfig::GetConfigPathUsed() );
#ifdef NDEBUG
        const bool buildDefaultValidation = false;
#else
        const bool buildDefaultValidation = true;
#endif
        app->SetEnableValidationLayers( UtilValidationConfig::ResolveEnabled( buildDefaultValidation ),
                                        UtilValidationConfig::GetRequestedLayerNames() );
        UtilStartupChecks::VerifyRequiredAssets();
        UtilLogger::Info( "APP", "Core configuration prepared." );

        UtilLogger::Info( "APP", "Entering engine run loop." );
        app->Run();
        UtilLogger::Info( "APP", "Engine exited run loop normally." );
    }
    catch ( const std::exception& e ) {
        UtilLogger::Error( "APP", std::string( "Unhandled exception: " ) + e.what() );
        std::cerr << e.what() << std::endl;
        UtilLogger::Shutdown();
        return EXIT_FAILURE;
    }

    UtilLogger::Info( "APP", "Application shutdown complete." );
    UtilLogger::Shutdown();
    return EXIT_SUCCESS;
}
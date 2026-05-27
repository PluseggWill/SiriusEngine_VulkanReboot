#include "Application.h"

#include "../Gfx/Gfx_SceneLoader.h"
#include "../RenderCore/Vk_Core.h"
#include "../Util/Util_AssetConfig.h"
#include "../Util/Util_AssetManifest.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_ValidationConfig.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void Application::Configure( uint32_t aWidth, uint32_t aHeight, const std::vector< const char* >& someDeviceExtensions ) {
    myWidth             = aWidth;
    myHeight            = aHeight;
    myDeviceExtensions  = someDeviceExtensions;
}

int Application::Run( int argc, char** argv ) {
    try {
        InitApp( argc, argv );
        LoadAndVerifyScene();

        Vk_Core& core = Vk_Core::GetInstance();
        UtilLogger::Info( "APP", "InitWindow." );
        core.InitWindow();
        UtilLogger::Info( "APP", "InitRenderDevice." );
        core.InitRenderDevice();
        UtilLogger::Info( "APP", "LoadSceneResources." );
        core.LoadSceneResources( std::move( mySceneDesc ) );

        RunMainLoop();

        UtilLogger::Info( "APP", "UnloadScene." );
        core.UnloadScene();
        UtilLogger::Info( "APP", "Shutdown." );
        core.Shutdown();
        UtilLogger::Info( "APP", "Engine exited run loop normally." );
        return EXIT_SUCCESS;
    }
    catch ( const std::exception& e ) {
        UtilLogger::Error( "APP", std::string( "Unhandled exception: " ) + e.what() );
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

void Application::InitApp( int argc, char** argv ) {
    UtilLogger::Info( "APP", "InitApp." );

    UtilValidationConfig::ParseCli( argc, argv );
    UtilAssetConfig::Initialize( argc, argv );
    UtilValidationConfig::LoadFromConfigFile( UtilAssetConfig::GetConfigPathUsed() );

    Vk_Core& core = Vk_Core::GetInstance();
    core.SetSize( myWidth, myHeight );
    core.SetRequiredExtension( myDeviceExtensions );

#ifdef NDEBUG
    const bool buildDefaultValidation = false;
#else
    const bool buildDefaultValidation = true;
#endif
    core.SetEnableValidationLayers( UtilValidationConfig::ResolveEnabled( buildDefaultValidation ),
                                    UtilValidationConfig::GetRequestedLayerNames() );
}

void Application::LoadAndVerifyScene() {
    UtilLogger::Info( "APP", "LoadSceneDesc." );
    mySceneDesc = Gfx_LoadSceneDesc( UtilAssetConfig::GetSceneLogicalPath() );
    UtilLogger::Info( "APP", "VerifyManifest." );
    Util_VerifyManifest( Util_CollectDependencies( mySceneDesc ) );
}

void Application::RunMainLoop() {
    Vk_Core& core = Vk_Core::GetInstance();
    UtilLogger::Info( "APP", "Entering main loop (Update/Render)." );
    while ( !core.ShouldClose() ) {
        float frameSeconds = 0.0f;
        core.Update( frameSeconds );
        core.Render();
    }
    UtilLogger::Info( "APP", "Main loop ended." );
}

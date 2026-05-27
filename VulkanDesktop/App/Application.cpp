#include "Application.h"

#include "../Gfx/Gfx_DemoSceneSim.h"
#include "../Gfx/Gfx_SceneLoader.h"
#include "../RenderCore/Vk_Core.h"
#include "../Util/Util_AssetManifest.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Logger.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void Application::Configure( const std::vector< const char* >& someDeviceExtensions ) {
    myDeviceExtensions = someDeviceExtensions;
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
    UtilEngineConfig::Initialize( argc, argv );
    UtilLogger::Init( UtilEngineConfig::GetLogFilePath() );
    UtilLogger::SetMinLogLevel( UtilEngineConfig::GetMinLogLevel() );

    UtilLogger::Info( "APP", "InitApp." );

    Vk_Core& core = Vk_Core::GetInstance();
    core.SetSize( UtilEngineConfig::GetWindowWidth(), UtilEngineConfig::GetWindowHeight() );
    core.SetVsync( UtilEngineConfig::GetVsync() );
    core.SetRequiredExtension( myDeviceExtensions );

#ifdef NDEBUG
    const bool buildDefaultValidation = false;
#else
    const bool buildDefaultValidation = true;
#endif
    const bool validationEnabled =
        UtilEngineConfig::ResolveValidationEnabled( buildDefaultValidation );
    core.SetEnableValidationLayers( validationEnabled, UtilEngineConfig::GetValidationLayerNames() );
    UtilEngineConfig::LogResolvedSummary();
}

void Application::LoadAndVerifyScene() {
    UtilLogger::Info( "APP", "LoadSceneDesc." );
    mySceneDesc = Gfx_LoadSceneDesc( UtilEngineConfig::GetSceneLogicalPath() );
    UtilLogger::Info( "APP", "VerifyManifest." );
    Util_VerifyManifest( Util_CollectDependencies( mySceneDesc ) );
}

void Application::RunMainLoop() {
    Vk_Core& core = Vk_Core::GetInstance();
    UtilLogger::Info( "APP", "Entering main loop (platform / input / render)." );
    while ( !core.ShouldClose() ) {
        float frameSeconds = 0.0f;
        core.BeginPlatformFrame( frameSeconds );
        myInput.Sample( core.GetWindow() );
        core.ApplyCameraInput( frameSeconds, myInput.GetSnapshot() );
        if ( myInput.HasLastSampleTime() ) {
            core.SetFrameInputSampleTime( myInput.GetLastSampleTime() );
        }
        Gfx_TickDemoSceneTransforms( core.GetSceneSoA(), core.GetDemoBaseTransforms() );
        core.Render();
    }
    UtilLogger::Info( "APP", "Main loop ended." );
}

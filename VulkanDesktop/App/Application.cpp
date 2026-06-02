#include "Application.h"

#include "ActiveViewsBuild.h"
#include "DebugOverlay.h"
#include "../Gfx/Gfx_DemoSceneSim.h"
#include "../Gfx/Gfx_SceneLoader.h"
#include "../Gfx/Gfx_SceneTransform.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../RenderCore/Vk_Core.h"
#include "../Util/Util_AssetManifest.h"
#include "../RenderCore/Vk_FrameCpuPrepResult.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_RenderDebugPanel.h"
#include <GLFW/glfw3.h>
#include <chrono>
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
        myLastLoadedScenePath = myConfig.GetSceneLogicalPath();
        core.LoadSceneResources( std::move( mySceneDesc ), myLastLoadedScenePath );

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
    myConfig.LoadFromArgv( argc, argv );
    UtilLogger::Init( myConfig.GetLogFilePath() );
    UtilLogger::SetMinLogLevel( myConfig.GetMinLogLevel() );

    Vk_Core& core = Vk_Core::GetInstance();
    core.BindEngineConfig( &myConfig );
    Gfx_ShaderPermutation::Initialize( myConfig );

    UtilLogger::Info( "APP", "InitApp." );

    core.BindWorldState( &myWorld );
    core.BindDebugUI( &myDebugUI );
    core.SetSize( myConfig.GetWindowWidth(), myConfig.GetWindowHeight() );
    core.SetVsync( myConfig.GetVsync() );
    core.SetRequiredExtension( myDeviceExtensions );
    core.ConfigureRenderDoc( myConfig.GetEnableRenderDoc() );

#ifdef NDEBUG
    const bool buildDefaultValidation = false;
#else
    const bool buildDefaultValidation = true;
#endif
    const bool validationEnabled = myConfig.ResolveValidationEnabled( buildDefaultValidation );
    core.SetEnableValidationLayers( validationEnabled, myConfig.GetValidationLayerNames() );
    myConfig.LogResolvedSummary();
}

void Application::LoadAndVerifyScene() {
    UtilLogger::Info( "APP", "LoadSceneDesc." );
    mySceneDesc = Gfx_LoadSceneDesc( myConfig, myConfig.GetSceneLogicalPath() );
    UtilLogger::Info( "APP", "VerifyManifest." );
    Util_VerifyManifest( myConfig, Util_CollectDependencies( mySceneDesc ), myConfig.GetAssetVerifyPolicy() );
}

void Application::RunMainLoop() {
    Vk_Core& core = Vk_Core::GetInstance();
    const int    smokeFrameLimit = myConfig.GetSmokeFrameLimit();
    const double smokeSeconds    = myConfig.GetSmokeSeconds();
    int          renderedFrames  = 0;
    const auto   smokeStart =
        ( smokeFrameLimit > 0 || smokeSeconds > 0.0 ) ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if ( smokeSeconds > 0.0 ) {
        UtilLogger::Info( "APP", "Smoke dwell: " + std::to_string( smokeSeconds ) + "s after scene load (main loop)." );
    }
    UtilLogger::Info( "APP", "Entering main loop (platform / input / render)." );
    while ( !core.ShouldClose() ) {
        float frameSeconds = 0.0f;
        core.BeginPlatformFrame( frameSeconds );
        myInput.Sample( core.GetWindow() );
        core.ApplyCameraInput( frameSeconds, myInput.GetSnapshot() );
        if ( myInput.HasLastSampleTime() ) {
            core.SetFrameInputSampleTime( myInput.GetLastSampleTime() );
        }
        const bool f12Pressed = glfwGetKey( core.GetWindow(), GLFW_KEY_F12 ) == GLFW_PRESS;
        if ( f12Pressed && !myRenderDocCaptureKeyDown ) {
            core.TriggerRenderDocCapture();
        }
        myRenderDocCaptureKeyDown = f12Pressed;
        Gfx_TickDemoSceneTransforms( myConfig, myWorld.mySceneTransformState );
        Gfx_ResolveFlatWorldTransforms( myWorld.mySceneTransformState, myWorld.mySceneSoA );

        uint32_t                viewCount = 0;
        const auto              views     = BuildActiveRenderViews( viewCount, myWorld, myDebugUI, core.GetFlyCamera(), core.GetSwapChainExtent() );
        Vk_FrameCpuPrepResult prep{};
        if ( core.PrepareFrameCpu( myWorld, views, viewCount, prep ) ) {
            UtilRenderDebugPanel::Build( myConfig, myDebugUI.myRenderDebug, core.GetEnvironmentData(), prep.myTotalOpaqueDraws, prep.myTotalTransparentDraws );
            BuildDebugOverlayPanels( myConfig, myDebugUI, myWorld, core, prep );
            if ( core.DrawFrameGpu( myDebugUI, prep ) == Vk_FrameResult::RequestShutdown ) {
                glfwSetWindowShouldClose( core.GetWindow(), GLFW_TRUE );
            }
        }

        TryProcessSceneReload();
        ++renderedFrames;

        const bool frameThresholdMet = smokeFrameLimit <= 0 || renderedFrames >= smokeFrameLimit;
        const bool timeThresholdMet  = smokeSeconds <= 0.0 ||
                                      std::chrono::duration<double>( std::chrono::steady_clock::now() - smokeStart ).count() >= smokeSeconds;
        const bool smokeExit         = ( smokeFrameLimit > 0 || smokeSeconds > 0.0 ) && frameThresholdMet && timeThresholdMet;
        if ( smokeExit ) {
            if ( smokeSeconds > 0.0 ) {
                UtilLogger::Info( "APP", "Smoke dwell reached (" + std::to_string( smokeSeconds ) + "s); requesting exit." );
            }
            if ( smokeFrameLimit > 0 ) {
                UtilLogger::Info( "APP", "Smoke frame limit reached (" + std::to_string( smokeFrameLimit ) + "); requesting exit." );
            }
            glfwSetWindowShouldClose( core.GetWindow(), GLFW_TRUE );
        }
    }
    UtilLogger::Info( "APP", "Main loop ended." );
}

std::string Application::TakePendingSceneReloadPath() {
    if ( !myDebugUI.myScenePanel.myReloadRequested ) {
        return {};
    }
    std::string path                           = std::move( myDebugUI.myScenePanel.myReloadTargetPath );
    myDebugUI.myScenePanel.myReloadRequested   = false;
    myDebugUI.myScenePanel.myReloadTargetPath.clear();
    return path;
}

void Application::TryProcessSceneReload() {
    Vk_Core& core = Vk_Core::GetInstance();
    const std::string reloadPath = TakePendingSceneReloadPath();
    if ( reloadPath.empty() ) {
        return;
    }

    UtilLogger::Info( "APP", "Scene reload requested: " + reloadPath );

    auto loadScene = [ & ]( const std::string& aPath ) {
        Gfx_SceneDesc desc = Gfx_LoadSceneDesc( myConfig, aPath );
        Util_VerifyManifest( myConfig, Util_CollectDependencies( desc ), myConfig.GetAssetVerifyPolicy() );
        core.LoadSceneResources( std::move( desc ), aPath );
        myLastLoadedScenePath = aPath;
    };

    try {
        core.UnloadScene();
        loadScene( reloadPath );
        UtilLogger::Info( "APP", "Scene reload completed: " + reloadPath );
    }
    catch ( const std::exception& e ) {
        UtilLogger::Error( "APP", std::string( "Scene reload failed: " ) + e.what() );
        try {
            if ( !myLastLoadedScenePath.empty() && myLastLoadedScenePath != reloadPath ) {
                UtilLogger::Warn( "APP", "Restoring previous scene: " + myLastLoadedScenePath );
                loadScene( myLastLoadedScenePath );
            }
        }
        catch ( const std::exception& restoreError ) {
            UtilLogger::Error( "APP", std::string( "Scene restore failed: " ) + restoreError.what() );
        }
    }
}

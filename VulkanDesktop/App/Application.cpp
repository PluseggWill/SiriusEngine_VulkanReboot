#include "Application.h"

#include "../Gfx/Gfx_DemoSceneSim.h"
#include "../Gfx/Gfx_FrameDebugToggles.h"
#include "../Gfx/Gfx_FramePrepInput.h"
#include "../Gfx/Gfx_ObjectiveRuntime.h"
#include "../Gfx/Gfx_SceneGpuLoadParams.h"
#include "../Gfx/Gfx_SceneLoader.h"
#include "../Gfx/Gfx_SceneTransform.h"
#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Gfx/Gfx_ViewPacketBuild.h"
#include "../RenderCore/Vk_FrameCpuPrepResult.h"
#include "../RenderCore/Vk_Renderer.h"
#include "../Util/Util_AssetManifest.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_ScenePanel.h"
#include "../Util/Util_TuningPanel.h"
#include "ActiveViewsBuild.h"
#include "App_PlatformHost.h"
#include "DebugOverlay.h"
#include "SceneCpuLoad.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void ProcessAppKeyboardShortcuts( GLFWwindow* aWindow, DebugUIState& aDebugUI, const std::string& aLoadedScenePath, bool& aRenderDocCaptureKeyDown, bool& aRestartKeyDown,
                                  Vk_Renderer& aRenderer ) {
    if ( ImGui::GetIO().WantCaptureKeyboard ) {
        aRenderDocCaptureKeyDown = false;
        aRestartKeyDown          = false;
        return;
    }

    const bool f12Pressed = glfwGetKey( aWindow, GLFW_KEY_F12 ) == GLFW_PRESS;
    if ( f12Pressed && !aRenderDocCaptureKeyDown ) {
        aRenderer.TriggerRenderDocCapture();
    }
    aRenderDocCaptureKeyDown = f12Pressed;

    const bool restartPressed = glfwGetKey( aWindow, GLFW_KEY_R ) == GLFW_PRESS;
    if ( restartPressed && !aRestartKeyDown && !aLoadedScenePath.empty() ) {
        UtilScenePanel::RequestReload( aDebugUI.myScenePanel, aLoadedScenePath );
    }
    aRestartKeyDown = restartPressed;
}

Gfx_FramePrepInput BuildFramePrepInput( WorldState& aWorld ) {
    Gfx_FramePrepInput input{};
    input.myScene                 = &aWorld.mySceneSoA;
    input.myLodTable              = &aWorld.myLodTable;
    input.myLodState              = &aWorld.myLodState;
    input.myLodDebugLogicalMeshId = aWorld.myLodDebugLogicalMeshId;
    return input;
}

void CommitSceneToWorld( WorldState& aWorld, Gfx_SceneDesc aScene, std::string aLogicalPath ) {
    aWorld.myLoadedScene    = std::move( aScene );
    aWorld.myLogicalPath    = std::move( aLogicalPath );
    aWorld.myHasLoadedScene = true;
    App_LoadSceneCpuState( aWorld );
}

void ActivateSceneGpu( Vk_Renderer& aRenderer, WorldState& aWorld, DebugUIState& aDebugUI, const Util_EngineConfig& aConfig, const std::string& aLogicalPath ) {
    aDebugUI.myScenePanel.myCurrentScenePath = aLogicalPath;
    UtilScenePanel::RefreshSceneList( aConfig, aDebugUI.myScenePanel );
    Gfx_SceneGpuLoadParams loadParams{};
    loadParams.mySceneDesc     = &aWorld.myLoadedScene;
    loadParams.mySceneIdTables = &aWorld.mySceneIdTables;
    loadParams.mySceneSoA      = &aWorld.mySceneSoA;
    loadParams.myLogicalPath   = aWorld.myLogicalPath;
    aRenderer.LoadSceneGpuResources( loadParams );
    App_InitScenePresentation( aRenderer, aWorld );
    // user-tuning.json overrides scene env + renderer session knobs (lighting/AO/post/camera).
    UtilTuningPanel::LoadAndApplyIfPresent( aConfig.GetAssetRoot(), aRenderer, aDebugUI );
}

}  // namespace

void Application::Configure( const std::vector< const char* >& someDeviceExtensions ) {
    myDeviceExtensions = someDeviceExtensions;
}

int Application::Run( int argc, char** argv ) {
    try {
        auto renderer = std::make_unique< Vk_Renderer >();
        myRenderer    = renderer.get();

        InitApp( argc, argv );
        LoadAndVerifyScene();

        Vk_Renderer&     rendererRef = *myRenderer;
        App_PlatformHost platformHost;
        myPlatformHost = &platformHost;
        rendererRef.BindPlatformHost( myPlatformHost );
        UtilLogger::Info( "APP", "InitWindow." );
        platformHost.InitWindow( myConfig.GetWindowWidth(), myConfig.GetWindowHeight(), rendererRef );
        rendererRef.SetPlatformWindow( platformHost.GetWindow() );
        UtilLogger::Info( "APP", "InitRenderDevice." );
        rendererRef.InitRenderDevice();
        UtilLogger::Info( "APP", "LoadScene." );
        myLastLoadedScenePath = myConfig.GetSceneLogicalPath();
        CommitSceneToWorld( myWorld, std::move( mySceneDesc ), myLastLoadedScenePath );
        ActivateSceneGpu( rendererRef, myWorld, myDebugUI, myConfig, myLastLoadedScenePath );
        Gfx_ResetObjectiveRuntime( myDebugUI.myObjectiveRuntime );

        RunMainLoop();

        UtilLogger::Info( "APP", "UnloadScene." );
        rendererRef.UnloadSceneGpuResources();
        myWorld.ClearCpuSceneState();
        myDebugUI.myScenePanel.myCurrentScenePath.clear();
        UtilLogger::Info( "APP", "Shutdown." );
        rendererRef.Shutdown();
        platformHost.ShutdownWindow();
        myPlatformHost = nullptr;
        myRenderer     = nullptr;
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

    Vk_Renderer& renderer = *myRenderer;
    renderer.BindEngineConfig( &myConfig );
    Gfx_ShaderPermutation::Initialize( myConfig );

    UtilLogger::Info( "APP", "InitApp." );

    renderer.SetSize( myConfig.GetWindowWidth(), myConfig.GetWindowHeight() );
    renderer.SetVsync( myConfig.GetVsync() );
    renderer.SetRequiredExtension( myDeviceExtensions );
    renderer.ConfigureRenderDoc( myConfig.GetEnableRenderDoc() );

#ifdef NDEBUG
    const bool buildDefaultValidation = false;
#else
    const bool buildDefaultValidation = true;
#endif
    const bool validationEnabled = myConfig.ResolveValidationEnabled( buildDefaultValidation );
    renderer.SetEnableValidationLayers( validationEnabled, myConfig.GetValidationLayerNames() );
    myConfig.LogResolvedSummary();
    myDebugUI.myRenderDebug.myLodEnabled = myConfig.GetFeatures().myLodEnabled;
}

void Application::LoadAndVerifyScene() {
    UtilLogger::Info( "APP", "LoadSceneDesc." );
    mySceneDesc = Gfx_LoadSceneDesc( myConfig, myConfig.GetSceneLogicalPath() );
    UtilLogger::Info( "APP", "VerifyManifest." );
    Util_VerifyManifest( myConfig, Util_CollectDependencies( mySceneDesc ), myConfig.GetAssetVerifyPolicy() );
}

void Application::RunMainLoop() {
    Vk_Renderer&      renderer        = *myRenderer;
    App_PlatformHost& platformHost    = *myPlatformHost;
    const int         smokeFrameLimit = myConfig.GetSmokeFrameLimit();
    const double      smokeSeconds    = myConfig.GetSmokeSeconds();
    int               renderedFrames  = 0;
    const auto        smokeStart      = ( smokeFrameLimit > 0 || smokeSeconds > 0.0 ) ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    if ( smokeSeconds > 0.0 ) {
        UtilLogger::Info( "APP", "Smoke dwell: " + std::to_string( smokeSeconds ) + "s after scene load (main loop)." );
    }
    UtilLogger::Info( "APP", "Entering main loop (platform / input / render)." );
    while ( !platformHost.ShouldClose() ) {
        float frameSeconds = 0.0f;
        platformHost.BeginFrame( renderer, frameSeconds );
        myInput.Sample( platformHost.GetWindow() );
        platformHost.BeginImGuiFrame( renderer );
        renderer.ApplyCameraInput( frameSeconds, myInput.GetSnapshot(), myDebugUI.myCameraSettings );
        if ( myInput.HasLastSampleTime() ) {
            renderer.SetFrameInputSampleTime( myInput.GetLastSampleTime() );
        }

        Gfx_TickDemoSceneTransforms( myConfig, myWorld.mySceneTransformState );
        Gfx_ResolveFlatWorldTransforms( myWorld.mySceneTransformState, myWorld.mySceneSoA );
        Gfx_TickObjectiveRuntime( myWorld.myLoadedScene.myObjective, renderer.GetFlyCamera().GetEye(), frameSeconds, myDebugUI.myObjectiveRuntime );

        uint32_t                    viewCount = 0;
        const auto                  views     = BuildActiveRenderViews( viewCount, myWorld, myDebugUI, renderer.GetFlyCamera(), renderer.GetSwapChainExtent() );
        Gfx_FramePrepInput          prepInput = BuildFramePrepInput( myWorld );
        const Gfx_FrameDebugToggles toggles   = Gfx_FrameDebugTogglesFromRenderDebug( myDebugUI.myRenderDebug.mySkipOpaquePass, myDebugUI.myRenderDebug.mySkipTransparentPass,
                                                                                      myDebugUI.myRenderDebug.myLodEnabled );
        std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews > viewPackets{};
        Gfx_FrameDrawStreamLogState                             streamLogs{};
        const uint32_t                                          activeViewCount = std::min( viewCount, kGfxMaxRenderViews );
        for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
            Gfx_LodState  secondaryViewLodState;
            Gfx_LodState* lodStateForView = prepInput.myLodState;
            if ( viewIndex > 0 ) {
                secondaryViewLodState = *prepInput.myLodState;
                lodStateForView       = &secondaryViewLodState;
            }
            Gfx_ViewPacketBuildParams packetParams{};
            packetParams.myScene                 = prepInput.myScene;
            packetParams.myView                  = views[ viewIndex ].myCamera.myView;
            packetParams.myProj                  = views[ viewIndex ].myCamera.myProj;
            packetParams.myCameraEye             = views[ viewIndex ].myCamera.myEye;
            packetParams.myViewLayerMask         = views[ viewIndex ].myView.myLayerMask;
            packetParams.myLodTable              = prepInput.myLodTable;
            packetParams.myLodState              = lodStateForView;
            packetParams.myLodEnabled            = toggles.myLodEnabled;
            packetParams.myLodDebugLogicalMeshId = prepInput.myLodDebugLogicalMeshId;
            packetParams.myGpuCullEnabled        = myConfig.GetGpuCullEnabled();
            size_t drawCountBeforeCull           = 0;
            Gfx_BuildViewFramePacket( packetParams, streamLogs, viewPackets[ viewIndex ], drawCountBeforeCull );
        }
        Vk_FrameCpuPrepResult prep{};
        if ( renderer.PrepareFrameCpu( prepInput, toggles, views, viewCount, viewPackets, prep ) ) {
            BuildDebugOverlayPanels( myConfig, myDebugUI, myWorld, renderer, prep );
            ProcessAppKeyboardShortcuts( platformHost.GetWindow(), myDebugUI, myLastLoadedScenePath, myRenderDocCaptureKeyDown, myRestartKeyDown, renderer );

            if ( renderer.DrawFrameGpu( toggles, prep ) == Vk_FrameResult::RequestShutdown ) {
                platformHost.RequestClose();
            }
        }

        TryProcessSceneReload();
        ++renderedFrames;

        const bool frameThresholdMet = smokeFrameLimit <= 0 || renderedFrames >= smokeFrameLimit;
        const bool timeThresholdMet  = smokeSeconds <= 0.0 || std::chrono::duration< double >( std::chrono::steady_clock::now() - smokeStart ).count() >= smokeSeconds;
        const bool smokeExit         = ( smokeFrameLimit > 0 || smokeSeconds > 0.0 ) && frameThresholdMet && timeThresholdMet;
        if ( smokeExit ) {
            if ( smokeSeconds > 0.0 ) {
                UtilLogger::Info( "APP", "Smoke dwell reached (" + std::to_string( smokeSeconds ) + "s); requesting exit." );
            }
            if ( smokeFrameLimit > 0 ) {
                UtilLogger::Info( "APP", "Smoke frame limit reached (" + std::to_string( smokeFrameLimit ) + "); requesting exit." );
            }
            platformHost.RequestClose();
        }
    }
    UtilLogger::Info( "APP", "Main loop ended." );
}

std::string Application::TakePendingSceneReloadPath() {
    if ( !myDebugUI.myScenePanel.myReloadRequested ) {
        return {};
    }
    std::string path                         = std::move( myDebugUI.myScenePanel.myReloadTargetPath );
    myDebugUI.myScenePanel.myReloadRequested = false;
    myDebugUI.myScenePanel.myReloadTargetPath.clear();
    return path;
}

void Application::TryProcessSceneReload() {
    Vk_Renderer&      renderer   = *myRenderer;
    const std::string reloadPath = TakePendingSceneReloadPath();
    if ( reloadPath.empty() ) {
        return;
    }

    UtilLogger::Info( "APP", "Scene reload requested: " + reloadPath );

    auto loadScene = [ & ]( const std::string& aPath ) {
        Gfx_SceneDesc desc = Gfx_LoadSceneDesc( myConfig, aPath );
        Util_VerifyManifest( myConfig, Util_CollectDependencies( desc ), myConfig.GetAssetVerifyPolicy() );
        CommitSceneToWorld( myWorld, std::move( desc ), aPath );
        ActivateSceneGpu( renderer, myWorld, myDebugUI, myConfig, aPath );
        myLastLoadedScenePath = aPath;
    };

    try {
        renderer.UnloadSceneGpuResources();
        myWorld.ClearCpuSceneState();
        loadScene( reloadPath );
        Gfx_ResetDemoSceneSimTime();
        Gfx_ResetObjectiveRuntime( myDebugUI.myObjectiveRuntime );
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

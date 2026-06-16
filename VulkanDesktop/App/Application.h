#pragma once

#include "../Gfx/Gfx_SceneDesc.h"
#include "../Util/Util_EngineConfig.h"
#include "DebugUIState.h"
#include "InputSystem.h"
#include "WorldState.h"
#include <vector>

class Vk_Renderer;
class App_PlatformHost;

// Application lifecycle: config → scene verify → device init → CPU/GPU scene load → loop → unload.
class Application {
public:
    void Configure( const std::vector< const char* >& someDeviceExtensions );

    // Returns process exit code (EXIT_SUCCESS / EXIT_FAILURE).
    int Run( int argc, char** argv );

private:
    void        InitApp( int argc, char** argv );
    void        LoadAndVerifyScene();
    void        RunMainLoop();
    void        TryProcessSceneReload();
    std::string TakePendingSceneReloadPath();

    std::vector< const char* > myDeviceExtensions;
    Util_EngineConfig          myConfig;  // Single source of truth; bound on Vk_Renderer in InitApp.
    WorldState                 myWorld;
    DebugUIState               myDebugUI;
    Gfx_SceneDesc              mySceneDesc;
    std::string                myLastLoadedScenePath;
    InputSystem                myInput;
    App_PlatformHost*          myPlatformHost            = nullptr;
    Vk_Renderer*               myRenderer                = nullptr;  // owned for app lifetime; set in Run()
    bool                       myRenderDocCaptureKeyDown = false;
    bool                       myRestartKeyDown          = false;
};

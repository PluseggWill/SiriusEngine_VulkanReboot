#pragma once

#include "../Gfx/Gfx_SceneDesc.h"
#include "InputSystem.h"
#include "WorldState.h"
#include <vector>

// Application lifecycle: config → scene verify → render device → load resources → loop → unload.
class Application {
public:
    void Configure( const std::vector< const char* >& someDeviceExtensions );

    // Returns process exit code (EXIT_SUCCESS / EXIT_FAILURE).
    int Run( int argc, char** argv );

private:
    void InitApp( int argc, char** argv );
    void LoadAndVerifyScene();
    void RunMainLoop();
    void TryProcessSceneReload();

    std::vector< const char* > myDeviceExtensions;
    WorldState                 myWorld;
    Gfx_SceneDesc              mySceneDesc;
    std::string                myLastLoadedScenePath;
    InputSystem                myInput;
    bool                       myRenderDocCaptureKeyDown = false;
};

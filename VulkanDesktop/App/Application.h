#pragma once

#include "../Gfx/Gfx_SceneDesc.h"
#include "InputSystem.h"
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

    std::vector< const char* > myDeviceExtensions;
    Gfx_SceneDesc              mySceneDesc;
    InputSystem                myInput;
};

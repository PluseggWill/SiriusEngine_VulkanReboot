#pragma once

#include "../Gfx/Gfx_SceneDesc.h"
#include <cstdint>
#include <vector>

// Application lifecycle: config → scene verify → render device → load resources → loop → unload.
class Application {
public:
    void Configure( uint32_t aWidth, uint32_t aHeight, const std::vector< const char* >& someDeviceExtensions );

    // Returns process exit code (EXIT_SUCCESS / EXIT_FAILURE).
    int Run( int argc, char** argv );

private:
    void InitApp( int argc, char** argv );
    void LoadAndVerifyScene();
    void RunMainLoop();

    uint32_t                     myWidth  = 1600;
    uint32_t                     myHeight = 1200;
    std::vector< const char* > myDeviceExtensions;
    Gfx_SceneDesc                mySceneDesc;
};

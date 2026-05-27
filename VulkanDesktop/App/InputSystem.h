#pragma once

#include "../Util/Util_InputSnapshot.h"

#include <chrono>

struct GLFWwindow;

// Application-layer input: GLFW poll → device-neutral snapshot for camera / future gameplay.
class InputSystem {
public:
    void Sample( GLFWwindow* aWindow );
    const Util_InputSnapshot& GetSnapshot() const { return mySnapshot; }

    // High-resolution time when Sample() finished (for input→present latency).
    std::chrono::high_resolution_clock::time_point GetLastSampleTime() const { return myLastSampleTime; }
    bool                                            HasLastSampleTime() const { return myHasLastSampleTime; }

private:
    Util_InputState     myState{};
    Util_InputSnapshot  mySnapshot{};
    std::chrono::high_resolution_clock::time_point myLastSampleTime{};
    bool                                            myHasLastSampleTime = false;
};

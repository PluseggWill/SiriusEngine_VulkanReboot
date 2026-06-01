#pragma once

// Device-neutral input for simulation / camera (no GLFW in consumers).
struct Util_InputSnapshot {
    bool  myMoveForward = false;
    bool  myMoveBack    = false;
    bool  myMoveLeft    = false;
    bool  myMoveRight   = false;
    bool  myMoveUp      = false;
    bool  myMoveDown    = false;
    float myMouseDeltaX = 0.0f;
    float myMouseDeltaY = 0.0f;
};

struct Util_CameraSettings {
    float myMoveSpeed        = 4.0f;
    float myMouseSensitivity = 0.002f;
};

// Persistent state for cursor capture / delta (owned by application loop).
struct Util_InputState {
    bool  myMouseLookActive     = false;
    bool  myFirstMouseLookFrame = true;
    float myLastCursorX         = 0.0f;
    float myLastCursorY         = 0.0f;
};

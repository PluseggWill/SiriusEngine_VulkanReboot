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

// Persistent state for RMB fly look (owned by application InputSystem).
struct Util_InputState {
    bool myMouseLookActive     = false;
    bool myFirstMouseLookFrame = true;  // skip delta on capture frame (avoid grab jump)
};

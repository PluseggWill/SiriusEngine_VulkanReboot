#pragma once

class Vk_Core;

// Platform frame slice: GLFW poll, delta time, ImGui bootstrap.
//
// Main-loop order (Application): BeginFrame → InputSystem::Sample → BeginImGuiFrame → ApplyCameraInput.
// ImGui NewFrame runs after input sample so RMB cursor recenter is not overwritten before camera consumes deltas.
class Vk_PlatformFrame {
public:
    static void InitWindow( Vk_Core& aCore );
    static void BeginFrame( Vk_Core& aCore, float& aOutDeltaSeconds );
    static void BeginImGuiFrame( Vk_Core& aCore );
};

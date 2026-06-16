#pragma once

class Vk_Renderer;

// Platform frame slice: GLFW poll, delta time, ImGui bootstrap.
//
// Main-loop order (Application): BeginFrame → InputSystem::Sample → BeginImGuiFrame → ApplyCameraInput.
// ImGui NewFrame runs after input sample so RMB cursor recenter is not overwritten before camera consumes deltas.
class Vk_PlatformFrame {
public:
    static void InitWindow( Vk_Renderer& aCore );
    static void BeginFrame( Vk_Renderer& aCore, float& aOutDeltaSeconds );
    static void BeginImGuiFrame( Vk_Renderer& aCore );
};

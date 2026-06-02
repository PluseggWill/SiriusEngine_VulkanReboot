#pragma once

class Vk_Core;

// Vk_PlatformFrame: owns GLFW poll/delta/imgui frame orchestration slice (phase-2 #9).
class Vk_PlatformFrame {
public:
    static void InitWindow( Vk_Core& aCore );
    static void BeginFrame( Vk_Core& aCore, float& aOutDeltaSeconds );
};

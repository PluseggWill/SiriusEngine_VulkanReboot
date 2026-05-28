#pragma once

class Vk_Core;

// Vk_SceneHost: owns scene CPU-state orchestration slice (phase-2 #8).
class Vk_SceneHost {
public:
    static void LoadCpuState( Vk_Core& aCore );
};

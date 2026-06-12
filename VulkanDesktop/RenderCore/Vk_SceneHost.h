#pragma once

class Vk_Core;
struct WorldState;

// Vk_SceneHost: owns scene CPU-state orchestration slice (phase-2 #8).
class Vk_SceneHost {
public:
    static void LoadCpuState( WorldState& aWorld, Vk_Core& aCore );

    // Fly camera + environment CPU defaults after scene load (first scene JSON camera when present).
    static void InitScenePresentation( Vk_Core& aCore, const WorldState& aWorld );
};

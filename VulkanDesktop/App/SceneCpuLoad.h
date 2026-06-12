#pragma once

class Vk_Core;
struct WorldState;

// Application-owned scene CPU bootstrap (was Vk_SceneHost). RenderCore must not mutate WorldState.
void App_LoadSceneCpuState( WorldState& aWorld );

// Camera + default env on Vk_Core after GPU scene load (needs swapchain extent from core).
void App_InitScenePresentation( Vk_Core& aCore, const WorldState& aWorld );

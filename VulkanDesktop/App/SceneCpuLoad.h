#pragma once

class Vk_Renderer;
struct WorldState;

// Application-owned scene CPU bootstrap (was Vk_SceneHost). RenderCore must not mutate WorldState.
void App_LoadSceneCpuState( WorldState& aWorld );

// Camera + default env on Vk_Renderer after GPU scene load (needs swapchain extent from core).
void App_InitScenePresentation( Vk_Renderer& aCore, const WorldState& aWorld );

// Default ambient/sun/fog on renderer; preserves myFogDistance.w (debug view from Render tab).
void App_ApplyDefaultEnvironment( Vk_Renderer& aCore );

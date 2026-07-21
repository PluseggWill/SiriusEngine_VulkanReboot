#pragma once

#include "../Util/Util_EngineConfig.h"

struct DebugUIState;
struct WorldState;
struct Vk_FrameCpuPrepResult;
class Vk_Renderer;
class Gfx_RenderCamera;

// ImGui panels that must run on the Application thread (no vk* in panel Build calls).
void BuildDebugOverlayPanels( const Util_EngineConfig& aConfig, DebugUIState& aDebugUI, const WorldState& aWorld, Vk_Renderer& aCore, const Gfx_RenderCamera& aFlyCamera,
                              const Vk_FrameCpuPrepResult& aPrep );

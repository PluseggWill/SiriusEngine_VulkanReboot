#pragma once

struct DebugUIState;
struct WorldState;
struct Vk_FrameCpuPrepResult;
class Vk_Core;

// ImGui panels that must run on the Application thread (no vk* in panel Build calls).
void BuildDebugOverlayPanels( DebugUIState& aDebugUI, const WorldState& aWorld, Vk_Core& aCore, const Vk_FrameCpuPrepResult& aPrep );

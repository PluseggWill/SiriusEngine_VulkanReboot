#pragma once

#include "../Gfx/Gfx_ActiveRenderView.h"
#include <array>

struct WorldState;
struct DebugUIState;
class Gfx_RenderCamera;

// Build API-agnostic active render views from world + debug UI + session fly camera.
std::array< Gfx_ActiveRenderView, kGfxMaxRenderViews > BuildActiveRenderViews( uint32_t& aOutViewCount, const WorldState& aWorld, const DebugUIState& aDebugUI,
                                                                               const Gfx_RenderCamera& aFlyCamera );

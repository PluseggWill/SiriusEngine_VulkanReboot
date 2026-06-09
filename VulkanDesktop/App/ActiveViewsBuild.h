#pragma once

#include "../Gfx/Gfx_RenderView.h"
#include "../RenderCore/Vk_ActiveRenderView.h"
#include <array>

struct WorldState;
struct DebugUIState;
class Vk_Camera;

// Build active render views from world + debug UI + session fly camera (no Vulkan device calls).
std::array< Vk_ActiveRenderView, kGfxMaxRenderViews > BuildActiveRenderViews( uint32_t& aOutViewCount, const WorldState& aWorld, const DebugUIState& aDebugUI,
                                                                              const Vk_Camera& aFlyCamera, VkExtent2D aSwapChainExtent );

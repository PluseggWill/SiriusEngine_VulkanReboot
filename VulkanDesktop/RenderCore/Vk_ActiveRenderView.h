#pragma once

#include "../Gfx/Gfx_RenderView.h"
#include "Vk_Camera.h"
#include <vulkan/vulkan.h>

// Resolved per-frame view: Gfx view mask + camera matrices + swapchain viewport/scissor (phase 3 peel).
struct Vk_ActiveRenderView {
    Gfx_RenderView myView;
    Vk_Camera      myCamera;
    VkViewport     myViewport{};
    VkRect2D       myScissor{};
};

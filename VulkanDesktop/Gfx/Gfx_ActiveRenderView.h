#pragma once

#include "Gfx_RenderView.h"

#include <glm/glm.hpp>

// API-agnostic resolved render view produced by App and consumed by RenderCore.
// Vulkan viewport/scissor conversion is owned by RenderCore.
struct Gfx_ActiveRenderView {
    Gfx_RenderView myView;
    glm::mat4      myCameraView{};
    glm::mat4      myCameraProj{};
    glm::vec3      myCameraEye{ 0.0f };
};

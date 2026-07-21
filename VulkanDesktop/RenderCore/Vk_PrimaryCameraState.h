#pragma once

#include <glm/glm.hpp>

// Primary-view matrices consumed by RenderCore passes (App owns Gfx_RenderCamera; sync each frame).
struct Vk_PrimaryCameraState {
    glm::mat4 myView{ 1.0f };
    glm::mat4 myProj{ 1.0f };
    glm::vec3 myEye{ 0.0f };
};

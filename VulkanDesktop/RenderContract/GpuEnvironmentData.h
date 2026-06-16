#pragma once

#include <glm/glm.hpp>

// std140 UBO, binding eVk_EnvBinding — field order must match EnvironmentData in lit/deferred shaders.
struct GpuEnvironmentData {
    glm::vec4 myFogColor;
    glm::vec4 myFogDistance;  // w = debug view mode
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;  // xyz toward sun
    glm::vec4 mySunlightColor;
    glm::vec4 myViewWorldPos;
};

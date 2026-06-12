#pragma once

// Shader-contract UBO structs + umbrella include for GPU scene resources (Vk_SceneResourceTypes.h).

#include "../Gfx/Gfx_MaterialTypes.h"
#include "../Gfx/Gfx_Vertex.h"
#include "Vk_AllocatedResource.h"
#include "Vk_SceneResourceTypes.h"

#include <glm/glm.hpp>

// std140 UBO, binding eVk_EnvBinding - field order must match EnvironmentData in TriangleFrag_Lit.frag.
struct GpuEnvironmentData {
    glm::vec4 myFogColor;     // reserved (fog not implemented in shader)
    glm::vec4 myFogDistance;  // x/y legacy Blinn-Phong (unused by PBR shaders); z=textureBlend; w=Gfx_DebugViewMode
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;  // xyz = direction from surface toward sun (normalized each frame)
    glm::vec4 mySunlightColor;
    glm::vec4 myViewWorldPos;  // xyz = camera world position (specular view vector)
};

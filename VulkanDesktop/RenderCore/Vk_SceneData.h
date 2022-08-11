#pragma once
#include <glm/glm.hpp>

struct GPUSceneData {
	glm::vec4 myFogColor;
    glm::vec4 myFogDistance;
    glm::vec4 myAmbientColor;
    glm::vec4 mySunlightDirection;
    glm::vec4 mySunlightColor;
};
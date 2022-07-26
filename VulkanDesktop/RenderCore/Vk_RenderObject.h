#pragma once
#include "Vk_Mesh.h"

struct Material {
    VkPipeline       myPipeline;
    VkPipelineLayout myPipelineLayout;
};

struct ModelMatrix {
    glm::mat4 model;
};

struct RenderObject {
    Mesh*     myMesh;
    Material* myMaterial;
    glm::mat4 myTransform;
};
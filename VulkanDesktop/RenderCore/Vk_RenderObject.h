#pragma once
#include "Vk_Mesh.h"
#include "Vk_Texture.h"

struct Material {
    VkPipeline       myPipeline;
    VkPipelineLayout myPipelineLayout;
};

struct RenderObject {
    Mesh*     myMesh;
    Material* myMaterial;
    glm::mat4 myTransformMatrix;
};
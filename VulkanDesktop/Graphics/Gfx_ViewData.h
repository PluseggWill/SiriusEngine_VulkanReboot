#pragma once
#include <array>
#include <vector>

#include "../RenderCore/Vk_Types.h"

class Gfx_ViewData {
    std::unordered_map< uint32_t, Material > myMaterialMap;
    std::unordered_map< uint32_t, Mesh >     myMeshMap;
    std::unordered_map< uint32_t, Texture >  myTextureMap;
    std::vector< RenderObject >              myRenderObjects;

    void Clear();
};

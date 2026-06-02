#pragma once
#include <array>
#include <vector>

#include "../RenderCore/Vk_Types.h"

class Gfx_ViewData {
    std::unordered_map< uint32_t, Gfx_Material > myMaterialMap;
    std::unordered_map< uint32_t, Gfx_Mesh >     myMeshMap;
    std::unordered_map< uint32_t, Gfx_Texture >  myTextureMap;
    std::vector< Gfx_RenderObject >              myRenderObjects;

    void Clear();
};

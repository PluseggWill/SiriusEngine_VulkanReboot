#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Gfx_Bounds.h"
#include "Gfx_Vertex.h"

// CPU mesh geometry loaded from disk — no Vulkan handles (RenderCore owns GPU buffers).
class Gfx_MeshCpu {
public:
    std::vector< Gfx_Vertex > myVertices;
    std::vector< uint32_t >   myIndices;
    Gfx_Bounds                myLocalBounds{};

    void LoadFromPath( const std::string& aPath );
};

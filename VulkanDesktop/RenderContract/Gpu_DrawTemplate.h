#pragma once

#include <cstdint>

// std430-friendly draw template: Vulkan indexed-indirect command + per-draw metadata.
// Gpu_DrawIndirectCommand mirrors VkDrawIndexedIndirectCommand (20 bytes); verified in RenderCore.
struct Gpu_DrawIndirectCommand {
    uint32_t indexCount    = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex    = 0;
    int32_t  vertexOffset  = 0;
    uint32_t firstInstance = 0;
};

struct Gpu_DrawTemplate {
    Gpu_DrawIndirectCommand myIndirect;
    uint32_t                myInstanceDataOffset = 0;
    uint32_t                myMeshId             = 0;
    uint32_t                myMaterialId         = 0;
    uint32_t                myEntityIndex        = 0;
};

static_assert( sizeof( Gpu_DrawIndirectCommand ) == 20, "Gpu_DrawIndirectCommand must match VkDrawIndexedIndirectCommand" );
static_assert( sizeof( Gpu_DrawTemplate ) == 36, "Gpu_DrawTemplate std430-friendly size" );

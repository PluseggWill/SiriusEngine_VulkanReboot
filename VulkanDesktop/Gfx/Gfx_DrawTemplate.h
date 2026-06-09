#pragma once

#include <cstdint>

#include "Gfx_DrawExtract.h"

// M2 draw template: Vulkan indexed-indirect command + per-draw metadata (CPU/GPU shared layout).
// Gfx_DrawIndirectCommand mirrors VkDrawIndexedIndirectCommand (20 bytes); verified in RenderCore.

struct Gfx_DrawIndirectCommand {
    uint32_t indexCount    = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex    = 0;
    int32_t  vertexOffset  = 0;
    uint32_t firstInstance = 0;
};

struct Gfx_DrawTemplate {
    Gfx_DrawIndirectCommand myIndirect;
    uint32_t                myInstanceDataOffset = 0;
    uint32_t                myMeshId             = 0;
    uint32_t                myMaterialId         = 0;
    uint32_t                myEntityIndex        = 0;
};

static_assert( sizeof( Gfx_DrawIndirectCommand ) == 20, "Gfx_DrawIndirectCommand must match VkDrawIndexedIndirectCommand" );
static_assert( sizeof( Gfx_DrawTemplate ) == 36, "Gfx_DrawTemplate std430-friendly size" );

// Fill one template from extract output + mesh index count (RenderCore resolves mesh buffers).
void Gfx_FillDrawTemplate( Gfx_DrawTemplate& aOut, const Gfx_DrawInstance& aDraw, uint32_t aIndexCount, uint32_t aInstanceDataOffset );

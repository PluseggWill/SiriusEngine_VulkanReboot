#pragma once

#include "../Gfx/Gfx_AoPass.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// Screen-space ambient occlusion — pluggable algorithms (Classic SSAO, HBAO+, GTAO).
struct Vk_AoState {
    Gfx_AoPass::PassState myGfx{};

    uint32_t myTemporalReadIndex    = 0u;
    bool     myTemporalHistoryReady = false;

    bool myInitialized = false;
};

namespace Vk_AoPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

void NoteRawAoLayout( VkImageLayout aLayout );

VkImageLayout GetRawAoLayout();

}  // namespace Vk_AoPass

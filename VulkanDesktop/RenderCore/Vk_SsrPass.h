#pragma once

#include "../Gfx/Gfx_SsrPass.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_SsrState {
    Gfx_SsrPass::PassState myGfx{};

    uint32_t myHistoryWriteIndex = 0u;  // index of buffer last written; SSR reads this buffer next frame
    // Pass-local: lit HDR history buffer has been written (AND with Vk_TemporalState::myHistoryValid).
    bool myHistoryReady = false;

    bool myInitialized = false;
};

namespace Vk_SsrPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );
void RecordHistoryUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );

}  // namespace Vk_SsrPass

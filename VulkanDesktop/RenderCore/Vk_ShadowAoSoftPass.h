#pragma once

#include "../Gfx/Gfx_ShadowAoSoftPass.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// Contact softening: pack linear AO (R) + screen-space sun shadow (G), bilateral blur.
// GPU resources live in myGfx; this shell keeps SPIR-V load + Init/Record orchestration.
struct Vk_ShadowAoSoftState {
    Gfx_ShadowAoSoftPass::PassState myGfx{};
    bool                            myInitialized = false;
};

namespace Vk_ShadowAoSoftPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex, bool aAoPassRan );

}  // namespace Vk_ShadowAoSoftPass

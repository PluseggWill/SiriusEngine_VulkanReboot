#pragma once

#include "../Gfx/Gfx_ClusterBuildPass.h"

#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

// FG v0: compute pass filling per-cluster light index lists (sun-only v1 stub).
struct Vk_ClusterBuildState {
    Gfx_ClusterBuildPass::PassState myGfx{};
    bool                            myInitialized = false;
};

namespace Vk_ClusterBuildPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordDispatch( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_ClusterBuildPass

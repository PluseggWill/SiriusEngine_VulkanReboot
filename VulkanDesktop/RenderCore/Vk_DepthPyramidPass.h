#pragma once

#include <cstdint>

#include "../Gfx/Gfx_DepthPyramidPass.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

constexpr uint32_t kHiZMaxMipLevels = 8u;

// Hi-Z min-depth pyramid from G-buffer depth. GPU resources live in myGfx.
struct Vk_DepthPyramidState {
    Gfx_DepthPyramidPass::PassState myGfx{};
    bool                            myInitialized = false;
};

namespace Vk_DepthPyramidPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordBuild( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

uint32_t GetMipLevelCount( const Vk_Renderer& aCore );

}  // namespace Vk_DepthPyramidPass

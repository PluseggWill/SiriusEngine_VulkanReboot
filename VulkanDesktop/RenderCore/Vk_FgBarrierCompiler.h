#pragma once

#include "Vk_DataStruct.h"

class Vk_Renderer;
struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;

namespace Vk_FgBarrierCompiler {

void CmdBarrierGBufferColorsForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );
void CmdBarrierGBufferDepthForShaderRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );
void CmdCopyGBufferDepthToSwapchain( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );

}  // namespace Vk_FgBarrierCompiler


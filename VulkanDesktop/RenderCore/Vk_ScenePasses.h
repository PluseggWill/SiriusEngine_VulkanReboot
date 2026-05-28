#pragma once

#include <cstdint>

#include "../Gfx/Gfx_RenderPacket.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;
class Vk_Core;

// Vk_ScenePasses: owns scene pass record orchestration slice (phase-2 #6).
class Vk_ScenePasses {
public:
    static void RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    static void RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    static void RecordDrawBatchesFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass );
    static void RecordDrawBatchesBindlessFromPacket( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, VkPipeline aPipeline );
};

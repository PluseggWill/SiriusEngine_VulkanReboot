#pragma once

#include <cstdint>
#include <vector>

#include "../Gfx/Gfx_DrawExtract.h"
#include "../Gfx/Gfx_DrawBatch.h"

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
    static void RecordDrawBatches( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, const std::vector< Gfx_BatchRun >& aBatchRuns );
    static void RecordDrawBatchesBindless( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_ExtractResult& aExtract, VkPipeline aPipeline );
};

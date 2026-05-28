#pragma once

class Vk_Core;

// Vk_GfxPipelineCache: owns scene pipeline orchestration slice (phase-2 #5).
class Vk_GfxPipelineCache {
public:
    static void InitScenePipelines( Vk_Core& aCore );
};

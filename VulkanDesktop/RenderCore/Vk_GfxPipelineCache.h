#pragma once

class Vk_Core;

// Vk_GfxPipelineCache: owns scene pipeline orchestration slice (phase-2 #5).
class Vk_GfxPipelineCache {
public:
    static void InitScenePipelines( Vk_Core& aCore );
    // Immediate destroy of scene graphics pipelines/layout (swapchain recreate or UnloadScene).
    static void DestroyScenePipelines( Vk_Core& aCore );
    static void CreateGfxPipeline( Vk_Core& aCore );
    static void CreateBindlessGfxPipelines( Vk_Core& aCore );
    static void CreateHybridResolveGfxPipelines( Vk_Core& aCore );
};

#pragma once

class Vk_Renderer;

// Vk_GfxPipelineCache: owns scene pipeline orchestration slice (phase-2 #5).
class Vk_GfxPipelineCache {
public:
    static void InitScenePipelines( Vk_Renderer& aCore );
    // Immediate destroy of scene graphics pipelines/layout (swapchain recreate or UnloadScene).
    static void DestroyScenePipelines( Vk_Renderer& aCore );
    static void CreateGfxPipeline( Vk_Renderer& aCore );
    static void CreateBindlessGfxPipelines( Vk_Renderer& aCore );
    static void CreateHybridResolveGfxPipelines( Vk_Renderer& aCore );
};

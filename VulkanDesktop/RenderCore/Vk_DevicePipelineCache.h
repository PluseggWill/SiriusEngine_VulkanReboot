#pragma once

class Vk_Renderer;

// Module: Vk_DevicePipelineCache — driver VkPipelineCache + disk persistence (S2).
// NOT Vk_GfxPipelineCache (that module only creates/destroys scene VkPipeline handles).
//
// Lifecycle: Create after logical device (Vk_RenderDevice::Init); Destroy in Vk_Renderer::Clear
// before vkDestroyDevice (writes Cache/pipeline_cache_v1.bin under asset root).
class Vk_DevicePipelineCache {
public:
    static void Create( Vk_Renderer& aCore );
    static void Destroy( Vk_Renderer& aCore );
};

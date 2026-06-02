#pragma once

class Vk_Core;

// Module: Vk_DevicePipelineCache — driver VkPipelineCache + disk persistence (S2).
// NOT Vk_GfxPipelineCache (that module only creates/destroys scene VkPipeline handles).
//
// Lifecycle: Create after logical device (Vk_RenderDevice::Init); Destroy in Vk_Core::Clear
// before vkDestroyDevice (writes Cache/pipeline_cache_v1.bin under asset root).
class Vk_DevicePipelineCache {
public:
    static void Create( Vk_Core& aCore );
    static void Destroy( Vk_Core& aCore );
};

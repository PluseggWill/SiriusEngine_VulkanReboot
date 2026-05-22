#pragma once

#include "Vk_DescriptorPolicy.h"

// Set indices - VkDescriptorPolicy::kSet*; pipeline uses set 0 only until S1.
enum eDescriptorSet : uint32_t {
    eVk_SetFrame    = VkDescriptorPolicy::kSetFrame,
    eVk_SetMaterial = VkDescriptorPolicy::kSetMaterial,
    eVk_SetObject   = VkDescriptorPolicy::kSetObject,
};

// Set 0 (Frame) - must match TriangleVertex.vert / TriangleFrag_Lit.frag layout(binding = N).
enum eDescriptorBinding {
    eVk_CameraBinding  = 0,  // VERTEX | GpuCameraData (view, proj; model is demo-only until S1)
    eVk_EnvBinding     = 1,  // FRAGMENT | GpuEnvironmentData (see myFogDistance packing)
    eVk_TextureBinding = 2,  // FRAGMENT | combined image sampler (albedo)
    eVk_BindingCount   = 3,
};
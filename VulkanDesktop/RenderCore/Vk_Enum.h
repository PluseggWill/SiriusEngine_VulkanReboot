#pragma once

#include "Vk_DescriptorPolicy.h"

// Set indices - VkDescriptorPolicy::kSet*; pipeline layout uses sets 0, 1 (placeholder), 2.
enum eDescriptorSet : uint32_t {
    eVk_SetFrame    = VkDescriptorPolicy::kSetFrame,
    eVk_SetMaterial = VkDescriptorPolicy::kSetMaterial,
    eVk_SetObject   = VkDescriptorPolicy::kSetObject,
};

// Set 0 (Frame) - TriangleVertex.vert set 0; TriangleFrag_Lit.frag env only (no albedo).
enum eDescriptorBinding {
    eVk_CameraBinding = 0,  // VERTEX | GpuCameraData (view, proj)
    eVk_EnvBinding    = 1,  // FRAGMENT | GpuEnvironmentData (see myFogDistance packing)
    eVk_FrameBindingCount = 2,
};

// Set 1 (Material) - TriangleFrag_Lit.frag set 1; bound once per material batch.
enum eVk_MaterialBinding : uint32_t {
    eVk_MaterialTextureBinding = 0,  // FRAGMENT | combined image sampler (albedo)
    eVk_MaterialAlphaBinding   = 1,  // FRAGMENT | GpuMaterialParams (alpha)
};

// Set 2 (Object) - TriangleVertex.vert binding 0; UNIFORM_BUFFER_DYNAMIC + dynamicOffset per draw.
enum eVk_ObjectBinding : uint32_t {
    eVk_ObjectModelBinding = 0,  // VERTEX | GpuObjectData
};

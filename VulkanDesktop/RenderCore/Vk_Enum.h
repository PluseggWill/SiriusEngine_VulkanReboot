#pragma once

// Global descriptor set (set 0) — must match TriangleVertex.vert / TriangleFrag_Lit.frag bindings.
enum eDescriptorBinding {
    eVk_CameraBinding  = 0,  // VERTEX | GpuCameraData (model, view, proj)
    eVk_EnvBinding     = 1,  // FRAGMENT | GpuEnvironmentData (see myFogDistance packing)
    eVk_TextureBinding = 2,  // FRAGMENT | combined image sampler (albedo)
    eVk_BindingCount   = 3,
};
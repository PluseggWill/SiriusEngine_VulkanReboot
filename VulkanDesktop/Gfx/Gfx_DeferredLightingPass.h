#pragma once

#include "../RenderContract/Gpu_ClusterPush.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Handles.h"

// Module: Gfx_DeferredLightingPass — hybrid resolve fullscreen triangle via Rhi (no vulkan.h).
// Caller must already be inside the hybrid graphics render pass (FG owns Begin/End).
namespace Gfx_DeferredLightingPass {

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
};

struct RecordInput {
    Gpu_DeferredLightingPushConstants myPush{};
    bool                              myDebugLabels = false;
};

void RecordDraw( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_DeferredLightingPass

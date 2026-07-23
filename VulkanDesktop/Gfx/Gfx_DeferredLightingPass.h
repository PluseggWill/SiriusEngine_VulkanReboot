#pragma once

#include "../RenderContract/Gpu_ClusterPush.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Handles.h"

#include <cstddef>
#include <cstdint>

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

struct PassState {
    Rhi_Pipeline myPipeline{};
    bool         myPipelineReady = false;
};

struct PipelineInitDesc {
    const void*        myVertSpirv      = nullptr;
    size_t             myVertSpirvBytes = 0;
    const void*        myFragSpirv      = nullptr;
    size_t             myFragSpirvBytes = 0;
    Rhi_PipelineLayout myLayout{};
    Rhi_RenderPass     myRenderPass{};
    uint32_t           mySampleCount = 1;
};

[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );
void               DestroyPipeline( Rhi_Device& aDevice, PassState& aState );

void RecordDraw( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_DeferredLightingPass

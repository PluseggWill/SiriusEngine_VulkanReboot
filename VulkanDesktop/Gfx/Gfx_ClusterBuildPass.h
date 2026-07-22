#pragma once

#include "../RenderContract/Gpu_ClusterPush.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Handles.h"

#include <cstdint>

// Module: Gfx_ClusterBuildPass — cluster light-list compute via Rhi (no vulkan.h).
namespace Gfx_ClusterBuildPass {

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_Buffer         myLightsBuffer{};
    Rhi_Buffer         myClusterListBuffer{};
};

struct RecordInput {
    uint32_t clusterCount = 0;
    uint32_t lightCount   = 0;
    bool     debugLabels  = false;
};

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_ClusterBuildPass

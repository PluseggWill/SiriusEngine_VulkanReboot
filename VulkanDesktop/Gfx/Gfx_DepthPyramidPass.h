#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstdint>

// Module: Gfx_DepthPyramidPass — Hi-Z min-depth mip build via Rhi (SSR / debug; not occlusion cull).
// mip0 = full-res G-buffer depth copy; subsequent mips = 2×2 min reduce.

namespace Gfx_DepthPyramidPass {

constexpr uint32_t kMaxMipLevels = 8u;

struct GpuResources {
    Rhi_Pipeline                                   myPipeline{};
    Rhi_PipelineLayout                             myLayout{};
    Rhi_Texture                                    myPyramid{};
    std::array< Rhi_DescriptorSet, kMaxMipLevels > myMipSets{};
    uint32_t                                       myMipLevelCount = 0;
};

struct RecordInput {
    uint32_t         myWidth         = 0;
    uint32_t         myHeight        = 0;
    bool             myDebugLabels   = false;
    Rhi_ImageLayout* myPyramidLayout = nullptr;  // inout tracked by RenderCore facade
};

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_DepthPyramidPass

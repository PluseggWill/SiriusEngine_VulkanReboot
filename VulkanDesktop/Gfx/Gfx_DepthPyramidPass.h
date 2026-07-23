#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstdint>

// Module: Gfx_DepthPyramidPass — Hi-Z min-depth mip Init + Record via Rhi (SSR / debug; not occlusion cull).
// mip0 = full-res G-buffer depth copy; subsequent mips = 2×2 min reduce.

namespace Gfx_DepthPyramidPass {

constexpr uint32_t kMaxMipLevels = 8u;
// Must be >= RenderCore MAX_FRAMES_IN_FLIGHT (currently 3).
constexpr uint32_t kMaxFramesInFlight = 3u;

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
    Rhi_ImageLayout* myPyramidLayout = nullptr;  // inout
};

struct PassState {
    Rhi_Pipeline                                                                     myPipeline{};
    Rhi_PipelineLayout                                                               myLayout{};
    Rhi_DescriptorSetLayout                                                          mySetLayout{};
    Rhi_DescriptorPool                                                               myPool{};
    Rhi_Sampler                                                                      myDepthSampler{};
    Rhi_Sampler                                                                      myPyramidSampler{};
    Rhi_Texture                                                                      myPyramid{};
    std::array< Rhi_Texture, kMaxMipLevels >                                         myMipViews{};
    std::array< std::array< Rhi_DescriptorSet, kMaxMipLevels >, kMaxFramesInFlight > mySets{};
    Rhi_ImageLayout                                                                  myPyramidLayout = Rhi_ImageLayout::Undefined;
    uint32_t                                                                         myMipLevelCount = 0;
    uint32_t                                                                         myWidth         = 0;
    uint32_t                                                                         myHeight        = 0;
    bool                                                                             mySetsAllocated = false;
    bool                                                                             myPipelineReady = false;
    bool                                                                             myInitialized   = false;
};

struct PipelineInitDesc {
    const void* mySpirvCode      = nullptr;
    size_t      mySpirvBytes     = 0;
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

struct ImageInitDesc {
    uint32_t    myWidth  = 0;
    uint32_t    myHeight = 0;
    Rhi_Texture myGBufferDepth{};  // adopted for descriptor CIS writes
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

// Creates compute PSO / layout / pool / samplers (no pyramid image yet).
[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );

// Allocates pyramid image + mip views + descriptor sets (or rewrites sets on recreate).
[[nodiscard]] bool CreateOrRecreateImage( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );

void DestroyImage( Rhi_Device& aDevice, PassState& aState );
void DestroyPipeline( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

[[nodiscard]] GpuResources MakeGpuResources( const PassState& aState, uint32_t aFrameIndex );

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_DepthPyramidPass

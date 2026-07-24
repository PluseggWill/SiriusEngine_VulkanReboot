#pragma once

#include "../RenderContract/Gpu_ClusterPush.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Module: Gfx_DeferredLightingPass — hybrid resolve fullscreen triangle via Rhi (no vulkan.h).
// Caller must already be inside the hybrid graphics render pass (FG owns Begin/End).
namespace Gfx_DeferredLightingPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

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
    Rhi_Pipeline                                        myPipeline{};
    Rhi_PipelineLayout                                  myLayout{};
    Rhi_DescriptorSetLayout                             mySetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Sampler                                         myGBufferSampler{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > mySets{};
    bool                                                myPipelineReady = false;
    bool                                                mySetsAllocated = false;
};

struct PipelineInitDesc {
    const void*    myVertSpirv      = nullptr;
    size_t         myVertSpirvBytes = 0;
    const void*    myFragSpirv      = nullptr;
    size_t         myFragSpirvBytes = 0;
    Rhi_RenderPass myRenderPass{};
    uint32_t       mySampleCount    = 1;
    uint32_t       myFramesInFlight = kMaxFramesInFlight;
};

// ~20 fragment-stage bindings (0-19); see UpdateDescriptorSet history in Vk_DeferredLightingPass.cpp for the source layout.
// Optional CIS inputs (shadow/IBL/AO/HiZ/DDGI/SSR/bent-normal) fall back to myFallback when empty (matches RC orSafe).
struct DescriptorUpdateDesc {
    uint32_t myFrameIndex = 0;

    Rhi_Texture myAlbedo{};           // binding 0
    Rhi_Texture myNormalRoughness{};  // binding 1
    Rhi_Texture myWorldPos{};         // binding 12
    Rhi_Texture myDepth{};            // binding 4

    Rhi_Buffer myLightsBuffer{};  // binding 2 (storage)
    uint64_t   myLightsRangeBytes = 0;
    Rhi_Buffer myClusterListBuffer{};  // binding 3 (storage, per-frame)

    Rhi_Buffer myLightingGlobals{};  // binding 5 (uniform)
    uint64_t   myLightingGlobalsOffsetBytes = 0;
    uint64_t   myLightingGlobalsRangeBytes  = 0;

    Rhi_Texture myShadowDepth{};  // binding 6 (compare sampler)
    Rhi_Sampler myShadowCompareSampler{};
    Rhi_Texture myShadowDepthRead{};  // binding 11 (non-compare read sampler)
    Rhi_Sampler myShadowDepthReadSampler{};

    Rhi_Texture myIrradianceIbl{};  // binding 7
    Rhi_Texture myPrefilterIbl{};   // binding 8
    Rhi_Texture myBrdfLut{};        // binding 9
    Rhi_Sampler myIblCubemapSampler{};
    Rhi_Sampler myBrdfLutSampler{};
    Rhi_Texture mySky{};  // binding 10

    Rhi_Texture myAo{};   // binding 13
    Rhi_Texture myHiZ{};  // binding 14
    Rhi_Sampler myHiZSampler{};

    Rhi_Texture myDdgiIrradiance{};  // binding 15
    Rhi_Texture myDdgiVisibility{};  // binding 16
    Rhi_Texture mySsr{};             // binding 17
    Rhi_Texture myBentNormal{};      // binding 18
    Rhi_Texture myMotionVector{};    // binding 19

    Rhi_Texture myFallback{};  // albedo fallback for empty optional CIS inputs
};

struct AoBindingUpdateDesc {
    uint32_t    myFrameIndex = 0;
    Rhi_Texture myAo{};
    Rhi_Texture myFallback{};
};

[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );
[[nodiscard]] bool CreateOrRecreateGraphicsPipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );
void               UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState );
// Fast path for the per-draw AO/contact rebind (binding 13) without a full descriptor rewrite.
void UpdateAoBinding( Rhi_Device& aDevice, const AoBindingUpdateDesc& aDesc, PassState& aState );

void DestroyPipeline( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

void RecordDraw( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_DeferredLightingPass

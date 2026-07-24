#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstdint>

// Module: Gfx_PostProcessPass — TAA / bloom / tonemap Init+Record via Rhi (no vulkan.h).
namespace Gfx_PostProcessPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

struct TonemapPush {
    float    exposure       = 1.0f;
    float    bloomIntensity = 0.0f;
    uint32_t tonemapEnabled = 1u;
    uint32_t bloomEnabled   = 0u;
    uint32_t tonemapMode    = 0u;
};
static_assert( sizeof( TonemapPush ) == 20, "TonemapPush must match Tonemap.frag" );

struct TaaPush {
    float historyBlend  = 0.0f;
    float historyValid  = 0.0f;
    float varianceGamma = 0.0f;
    float sharpen       = 0.0f;
};
static_assert( sizeof( TaaPush ) == 16, "TaaPush must match TaaResolve.comp" );

struct BloomGpu {
    Rhi_Pipeline       myThresholdPipeline{};
    Rhi_PipelineLayout myThresholdLayout{};
    Rhi_DescriptorSet  myThresholdSet{};
    Rhi_Pipeline       myBlurPipeline{};
    Rhi_PipelineLayout myBlurLayout{};
    Rhi_DescriptorSet  myBlurHorizSet{};
    Rhi_DescriptorSet  myBlurVertSet{};
    Rhi_Texture        mySceneColor{};
    Rhi_Texture        myBloomPing{};
    Rhi_Texture        myBloomPong{};
};

struct BloomInput {
    uint32_t         myWidth       = 0;
    uint32_t         myHeight      = 0;
    float            myThreshold   = 0.0f;
    Rhi_ImageLayout* mySceneLayout = nullptr;
    Rhi_ImageLayout* myPingLayout  = nullptr;
    Rhi_ImageLayout* myPongLayout  = nullptr;
};

struct TaaGpu {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_Texture        mySceneColor{};
    Rhi_Texture        myResolved{};
    Rhi_Texture        myHistoryRead{};
    Rhi_Texture        myHistoryWrite{};
};

struct TaaInput {
    uint32_t         myWidth  = 0;
    uint32_t         myHeight = 0;
    TaaPush          myPush{};
    Rhi_ImageLayout* mySceneLayout        = nullptr;
    Rhi_ImageLayout* myResolvedLayout     = nullptr;
    Rhi_ImageLayout* myHistoryReadLayout  = nullptr;
    Rhi_ImageLayout* myHistoryWriteLayout = nullptr;
};

struct TonemapGpu {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_RenderPass     myRenderPass{};
    Rhi_Framebuffer    myFramebuffer{};
    Rhi_Texture        mySceneColor{};
    Rhi_Texture        myBloomPing{};
};

struct TonemapInput {
    uint32_t         myWidth  = 0;
    uint32_t         myHeight = 0;
    TonemapPush      myPush{};
    bool             myDebugLabels  = false;
    bool             myBloomEnabled = false;
    Rhi_ImageLayout* mySceneLayout  = nullptr;
    Rhi_ImageLayout* myPingLayout   = nullptr;
};

void RecordBloom( Rhi_CommandList& aCmd, const BloomGpu& aGpu, BloomInput& aInput );
void RecordTaa( Rhi_CommandList& aCmd, const TaaGpu& aGpu, TaaInput& aInput );
void RecordTonemap( Rhi_CommandList& aCmd, const TonemapGpu& aGpu, TonemapInput& aInput );

struct PassState {
    Rhi_Texture mySceneColor{};
    Rhi_Texture myTaaResolved{};
    Rhi_Texture myTaaHistory0{};
    Rhi_Texture myTaaHistory1{};
    Rhi_Texture myBloomPing{};
    Rhi_Texture myBloomPong{};

    Rhi_Sampler             mySceneSampler{};
    Rhi_DescriptorSetLayout myThresholdSetLayout{};
    Rhi_DescriptorSetLayout myBlurSetLayout{};
    Rhi_DescriptorSetLayout myTaaSetLayout{};
    Rhi_PipelineLayout      myThresholdLayout{};
    Rhi_PipelineLayout      myBlurLayout{};
    Rhi_PipelineLayout      myTaaLayout{};
    Rhi_Pipeline            myThresholdPipeline{};
    Rhi_Pipeline            myBlurPipeline{};
    Rhi_Pipeline            myTaaPipeline{};
    Rhi_DescriptorPool      myPool{};

    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myThresholdSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurHorizSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurVertSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myTaaSets{};

    // Tonemap (swapchain graphics) — separate pool from bloom/TAA compute.
    Rhi_DescriptorSetLayout                             myTonemapSetLayout{};
    Rhi_PipelineLayout                                  myTonemapLayout{};
    Rhi_Pipeline                                        myTonemapPipeline{};
    Rhi_DescriptorPool                                  myTonemapPool{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myTonemapSets{};

    uint32_t myWidth                = 0;
    uint32_t myHeight               = 0;
    uint32_t myBloomWidth           = 0;
    uint32_t myBloomHeight          = 0;
    uint32_t myTaaHistoryWriteIndex = 0u;
    bool     myTaaHistoryReady      = false;  // false until first resolve copied into history
    bool     myImagesReady          = false;
    bool     myComputeReady         = false;
    bool     mySetsAllocated        = false;
    bool     myTonemapReady         = false;  // layouts + pool + sets
    bool     myTonemapPipelineReady = false;
};

struct ImageInitDesc {
    uint32_t myWidth  = 0;
    uint32_t myHeight = 0;
};

struct ComputeInitDesc {
    const void* myThresholdSpirv      = nullptr;
    size_t      myThresholdSpirvBytes = 0;
    const void* myBlurSpirv           = nullptr;
    size_t      myBlurSpirvBytes      = 0;
    const void* myTaaSpirv            = nullptr;
    size_t      myTaaSpirvBytes       = 0;
    uint32_t    myFramesInFlight      = kMaxFramesInFlight;
};

struct DescriptorUpdateDesc {
    uint32_t    myFramesInFlight      = kMaxFramesInFlight;
    uint32_t    myTaaHistoryReadIndex = 0;            // history buffer to sample
    uint32_t    myFrameIndex          = 0xffffffffu;  // 0xffffffff = all frames; else only that slot
    Rhi_Texture myMotionVector{};                     // optional for TAA; skip TAA writes if empty
};

struct TonemapInitDesc {
    const void*    myVertSpirv      = nullptr;
    size_t         myVertSpirvBytes = 0;
    const void*    myFragSpirv      = nullptr;
    size_t         myFragSpirvBytes = 0;
    Rhi_RenderPass myRenderPass{};  // swapchain RP (adopted OK)
    uint32_t       mySampleCount    = 1;
    uint32_t       myFramesInFlight = kMaxFramesInFlight;
};

struct TonemapDescriptorUpdateDesc {
    uint32_t myFramesInFlight = kMaxFramesInFlight;
    uint32_t myFrameIndex     = 0xffffffffu;  // 0xffffffff = all frames; else only that slot
    bool     myUseTaaResolved = false;
};

[[nodiscard]] bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );
[[nodiscard]] bool CreateComputeResources( Rhi_Device& aDevice, const ComputeInitDesc& aDesc, PassState& aState );
void               UpdateComputeDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState );

[[nodiscard]] bool CreateTonemapResources( Rhi_Device& aDevice, const TonemapInitDesc& aDesc, PassState& aState );
[[nodiscard]] bool CreateOrRecreateTonemapPipeline( Rhi_Device& aDevice, const TonemapInitDesc& aDesc, PassState& aState );
void               UpdateTonemapDescriptors( Rhi_Device& aDevice, const TonemapDescriptorUpdateDesc& aDesc, PassState& aState );

void DestroyImages( Rhi_Device& aDevice, PassState& aState );
void DestroyComputeResources( Rhi_Device& aDevice, PassState& aState );
void DestroyTonemapResources( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

// Legacy aliases used while RC migrates call sites.
using ComputePassState         = PassState;
using ComputePipelinesInitDesc = ComputeInitDesc;
[[nodiscard]] bool CreateComputePipelines( Rhi_Device& aDevice, const ComputePipelinesInitDesc& aDesc, ComputePassState& aState );
void               DestroyComputePipelines( Rhi_Device& aDevice, ComputePassState& aState );

}  // namespace Gfx_PostProcessPass

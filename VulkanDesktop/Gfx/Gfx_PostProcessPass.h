#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <cstdint>

// Module: Gfx_PostProcessPass — TAA / bloom / tonemap recording via Rhi (no vulkan.h).
namespace Gfx_PostProcessPass {

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

}  // namespace Gfx_PostProcessPass

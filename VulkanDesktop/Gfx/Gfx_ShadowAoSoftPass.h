#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstdint>

namespace Gfx_ShadowAoSoftPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

struct GpuResources {
    Rhi_Pipeline       myPackPipeline{};
    Rhi_PipelineLayout myPackLayout{};
    Rhi_Pipeline       myBlurPipeline{};
    Rhi_PipelineLayout myBlurLayout{};
    Rhi_DescriptorSet  myPackSet{};
    Rhi_DescriptorSet  myBlurHorizSet{};
    Rhi_DescriptorSet  myBlurVertSet{};
    Rhi_Texture        mySoftPing{};
    Rhi_Texture        mySoftPong{};
    Rhi_Texture        myGBufferWorldPos{};
    Rhi_Texture        myAoRaw{};  // optional when useAoRaw
};

struct RecordInput {
    uint32_t         myWidth       = 0;
    uint32_t         myHeight      = 0;
    bool             myDebugLabels = false;
    bool             myUseAoRaw    = false;
    float            myBlurRadius  = 1.0f;
    float            myDepthSigma  = 0.04f;
    Rhi_ImageLayout* myPingLayout  = nullptr;
    Rhi_ImageLayout* myPongLayout  = nullptr;
    Rhi_ImageLayout* myAoRawLayout = nullptr;  // optional inout when myUseAoRaw
};

struct PassState {
    Rhi_Pipeline                                        myPackPipeline{};
    Rhi_PipelineLayout                                  myPackLayout{};
    Rhi_Pipeline                                        myBlurPipeline{};
    Rhi_PipelineLayout                                  myBlurLayout{};
    Rhi_DescriptorSetLayout                             myPackSetLayout{};
    Rhi_DescriptorSetLayout                             myBlurSetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Sampler                                         myGBufferSampler{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myPackSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myPackNoAoSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurHorizSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurVertSets{};
    Rhi_Texture                                         mySoftPing{};
    Rhi_Texture                                         mySoftPong{};
    Rhi_Texture                                         myFallbackAo{};
    Rhi_Texture                                         myFallbackContact{};
    Rhi_ImageLayout                                     mySoftPingLayout = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout                                     mySoftPongLayout = Rhi_ImageLayout::Undefined;
    uint32_t                                            myWidth          = 0;
    uint32_t                                            myHeight         = 0;
    bool                                                myFallbackReady  = false;
    bool                                                myImagesReady    = false;
    bool                                                myPipelineReady  = false;
    bool                                                mySetsAllocated  = false;
};

struct PipelineInitDesc {
    const void* myPackSpirvCode  = nullptr;
    size_t      myPackSpirvBytes = 0;
    const void* myBlurSpirvCode  = nullptr;
    size_t      myBlurSpirvBytes = 0;
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

struct ImageInitDesc {
    uint32_t myWidth          = 0;
    uint32_t myHeight         = 0;
    bool     myCreateFallback = true;
};

struct DescriptorUpdateDesc {
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
    Rhi_Texture myGBufferDepth{};
    Rhi_Texture myGBufferWorldPos{};
    Rhi_Texture myAoRaw{};  // optional; pack-with-AO uses this when set
    Rhi_Texture myShadowDepth{};
    Rhi_Sampler myShadowCompareSampler{};
    Rhi_Buffer  myLightingGlobals{};
    uint64_t    myLightingGlobalsStride = 0;
    uint64_t    myLightingGlobalsRange  = 0;
};

[[nodiscard]] bool CreatePipelines( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );

[[nodiscard]] bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState );

void DestroyImages( Rhi_Device& aDevice, PassState& aState );
void DestroyPipelines( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_ShadowAoSoftPass

#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"
#include "Gfx_AoSettings.h"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstdint>

// Module: Gfx_AoPass — AO compute Init + Record via Rhi (no vulkan.h).

namespace Gfx_AoPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

struct GpuResources {
    Rhi_Pipeline       myClassicPipeline{};
    Rhi_Pipeline       myHbaoPipeline{};
    Rhi_Pipeline       myGtaoPipeline{};
    Rhi_Pipeline       myUpsamplePipeline{};
    Rhi_Pipeline       myBlurPipeline{};
    Rhi_Pipeline       myTemporalPipeline{};
    Rhi_PipelineLayout myClassicLayout{};
    Rhi_PipelineLayout myHalfResLayout{};
    Rhi_PipelineLayout myUpsampleLayout{};
    Rhi_PipelineLayout myBlurLayout{};
    Rhi_PipelineLayout myTemporalLayout{};
    Rhi_DescriptorSet  myClassicSet{};
    Rhi_DescriptorSet  myHalfResSet{};
    Rhi_DescriptorSet  myUpsampleSet{};
    Rhi_DescriptorSet  myBlurHorizSet{};
    Rhi_DescriptorSet  myBlurVertSet{};
    Rhi_DescriptorSet  myTemporalSet{};
    Rhi_Texture        myAoRaw{};
    Rhi_Texture        myAoHalf{};
    Rhi_Texture        myBentNormalHalf{};
    Rhi_Texture        myAoBlur{};
    Rhi_Texture        myAoHistory0{};
    Rhi_Texture        myAoHistory1{};
    Rhi_Texture        myGBufferNormal{};
    Rhi_Texture        myGBufferWorldPos{};
};

struct LayoutState {
    Rhi_ImageLayout myAoRaw    = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout myAoHalf   = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout myBentHalf = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout myAoBlur   = Rhi_ImageLayout::Undefined;
};

struct RecordInput {
    Gfx_AoSettings mySettings{};
    glm::mat4      myView{ 1.0f };
    glm::mat4      myProj{ 1.0f };
    uint32_t       myWidth                                                      = 0;
    uint32_t       myHeight                                                     = 0;
    uint32_t       myFrameIndex                                                 = 0;
    bool           myDebugLabels                                                = false;
    bool           myContactSoftActive                                          = false;
    bool           mySharedTemporalHistoryValid                                 = false;
    uint32_t*      myTemporalReadIndex                                          = nullptr;
    bool*          myTemporalHistoryReady                                       = nullptr;
    LayoutState*   myLayouts                                                    = nullptr;
    void ( *myPrepareTemporalDescriptors )( void* aUser, uint32_t aFrameIndex ) = nullptr;
    void* myPrepareTemporalUser                                                 = nullptr;
};

struct PassState {
    Rhi_Pipeline                                        myClassicPipeline{};
    Rhi_Pipeline                                        myHbaoPipeline{};
    Rhi_Pipeline                                        myGtaoPipeline{};
    Rhi_Pipeline                                        myUpsamplePipeline{};
    Rhi_Pipeline                                        myBlurPipeline{};
    Rhi_Pipeline                                        myTemporalPipeline{};
    Rhi_PipelineLayout                                  myClassicLayout{};
    Rhi_PipelineLayout                                  myHalfResLayout{};
    Rhi_PipelineLayout                                  myUpsampleLayout{};
    Rhi_PipelineLayout                                  myBlurLayout{};
    Rhi_PipelineLayout                                  myTemporalLayout{};
    Rhi_DescriptorSetLayout                             myClassicSetLayout{};
    Rhi_DescriptorSetLayout                             myHalfResSetLayout{};
    Rhi_DescriptorSetLayout                             myUpsampleSetLayout{};
    Rhi_DescriptorSetLayout                             myBlurSetLayout{};
    Rhi_DescriptorSetLayout                             myTemporalSetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Sampler                                         myGBufferSampler{};
    Rhi_Texture                                         myAoRaw{};
    Rhi_Texture                                         myAoHalf{};
    Rhi_Texture                                         myBentNormalHalf{};
    Rhi_Texture                                         myAoBlur{};
    Rhi_Texture                                         myAoHistory0{};
    Rhi_Texture                                         myAoHistory1{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myClassicSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myHalfResSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myUpsampleSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurHorizSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myBlurVertSets{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > myTemporalSets{};
    uint32_t                                            myWidth         = 0;
    uint32_t                                            myHeight        = 0;
    bool                                                mySetsAllocated = false;
    bool                                                myPipelineReady = false;
    bool                                                myImagesReady   = false;
};

struct PipelineInitDesc {
    const void* myClassicSpirvCode   = nullptr;
    size_t      myClassicSpirvBytes  = 0;
    const void* myHbaoSpirvCode      = nullptr;
    size_t      myHbaoSpirvBytes     = 0;
    const void* myGtaoSpirvCode      = nullptr;
    size_t      myGtaoSpirvBytes     = 0;
    const void* myUpsampleSpirvCode  = nullptr;
    size_t      myUpsampleSpirvBytes = 0;
    const void* myBlurSpirvCode      = nullptr;
    size_t      myBlurSpirvBytes     = 0;
    const void* myTemporalSpirvCode  = nullptr;
    size_t      myTemporalSpirvBytes = 0;
    uint32_t    myFramesInFlight     = kMaxFramesInFlight;
};

struct ImageInitDesc {
    uint32_t myWidth          = 0;
    uint32_t myHeight         = 0;
    uint32_t myFramesInFlight = kMaxFramesInFlight;
};

[[nodiscard]] bool CreatePipelines( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );
[[nodiscard]] bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );

void DestroyImages( Rhi_Device& aDevice, PassState& aState );
void DestroyPipelines( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_AoPass

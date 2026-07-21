#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"
#include "Gfx_AoSettings.h"

#include <glm/mat4x4.hpp>

#include <cstdint>

// Module: Gfx_AoPass — AO compute record orchestration via Rhi (no vulkan.h).
// Pipeline/descriptor/texture creation remains in RenderCore Vk_AoPass until E4.

namespace Gfx_AoPass {

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
    uint32_t       myWidth                      = 0;
    uint32_t       myHeight                     = 0;
    uint32_t       myFrameIndex                 = 0;
    bool           myDebugLabels                = false;
    bool           myContactSoftActive          = false;
    bool           mySharedTemporalHistoryValid = false;
    uint32_t*      myTemporalReadIndex          = nullptr;
    bool*          myTemporalHistoryReady       = nullptr;
    LayoutState*   myLayouts                    = nullptr;
    // RenderCore fills temporal descriptor set before temporal dispatch.
    void ( *myPrepareTemporalDescriptors )( void* aUser, uint32_t aFrameIndex ) = nullptr;
    void* myPrepareTemporalUser                                                 = nullptr;
};

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_AoPass

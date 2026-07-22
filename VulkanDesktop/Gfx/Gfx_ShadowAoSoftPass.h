#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <cstdint>

namespace Gfx_ShadowAoSoftPass {

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

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput );

}  // namespace Gfx_ShadowAoSoftPass

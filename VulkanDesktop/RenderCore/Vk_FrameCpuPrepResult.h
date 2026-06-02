#pragma once

#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_RenderView.h"
#include "Vk_FrameData.h"
#include <array>

// Output of Vk_Core::PrepareFrameCpu — consumed by Application panels then DrawFrameGpu.
struct Vk_FrameCpuPrepResult {
    bool     myOk                    = false;
    uint32_t myImageIndex            = 0;
    uint32_t myActiveViewCount       = 0;
    uint32_t myTotalOpaqueDraws      = 0;
    uint32_t myTotalTransparentDraws = 0;
    float    myGpuFenceWaitMs        = 0.0f;

    Vk_FrameData* myFrameData = nullptr;

    std::array< Gfx_FrameRenderPacket, kGfxMaxRenderViews > myViewPackets{};
    std::array< VkViewport, kGfxMaxRenderViews >            myViewports{};
    std::array< VkRect2D, kGfxMaxRenderViews >              myScissors{};
    std::array< VkDescriptorSet, kGfxMaxRenderViews >       myFrameDescriptors{};
};

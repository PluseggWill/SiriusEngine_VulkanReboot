#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Handles.h"

#include <glm/mat4x4.hpp>

#include <cstdint>

// Module: Gfx_ShadowMapPass — directional shadow depth RP + draws via Rhi (no vulkan.h).
// Depth pre-transition to DepthStencilAttachment stays in the RenderCore facade when needed.
namespace Gfx_ShadowMapPass {

constexpr uint32_t kMapSize = 2048u;

struct DrawItem {
    Rhi_Buffer myVertexBuffer{};
    Rhi_Buffer myIndexBuffer{};
    uint32_t   myIndexCount    = 0;
    uint32_t   myDynamicOffset = 0;
};

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  myObjectSet{};
    Rhi_RenderPass     myRenderPass{};
    Rhi_Framebuffer    myFramebuffer{};
};

struct RecordInput {
    glm::mat4       myLightViewProj{ 1.0f };
    float           myDepthBiasConstant = 0.0f;
    float           myDepthBiasSlope    = 0.0f;
    bool            myDebugLabels       = false;
    const DrawItem* myDraws             = nullptr;
    uint32_t        myDrawCount         = 0;
    uint32_t*       myDrawCallsOut      = nullptr;  // optional: increment per indexed draw
};

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_ShadowMapPass

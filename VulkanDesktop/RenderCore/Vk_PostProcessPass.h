#pragma once

#include "../Gfx/Gfx_PostProcessPass.h"

#include "../Rhi/Rhi_Handles.h"

#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

constexpr VkFormat kPostSceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

// HDR scene color + hybrid resolve RP/FB + tonemap/bloom (extent-sized; recreated on resize).
// GPU resources live in myGfx; this shell keeps WSI hybrid RP/FB + Init/Record orchestration.
struct Vk_PostProcessState {
    Gfx_PostProcessPass::PassState myGfx{};

    VkRenderPass    myHybridRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer   myHybridFramebuffer = VK_NULL_HANDLE;
    Rhi_RenderPass  myRhiHybridRenderPass{};
    Rhi_Framebuffer myRhiHybridFramebuffer{};

    Vk_DeletionQueue myDeletionQueue;
    bool             myInitialized = false;
};

namespace Vk_PostProcessPass {

bool HasHybridResolve( const Vk_Renderer& aCore );

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

// Hybrid resolve RP ends with sceneColor in SHADER_READ_ONLY_OPTIMAL — keep CPU layout tracker in sync.
void MarkSceneColorShaderRead();

void RecordPost( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aImageIndex, uint32_t aFrameIndex );

}  // namespace Vk_PostProcessPass

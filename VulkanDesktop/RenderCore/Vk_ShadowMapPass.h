#pragma once

#include "../Gfx/Gfx_ShadowMapPass.h"

#include "Vk_Types.h"

#include <glm/glm.hpp>

class Vk_Renderer;

struct Gfx_PassDrawPacket;

struct Vk_ShadowMapState {

    Gfx_ShadowMapPass::PassState myGfx{};

    bool myInitialized = false;

    static constexpr uint32_t kMapSize = 2048u;

    Vk_TextureResource myDepth{};

    VkRenderPass myRenderPass = VK_NULL_HANDLE;

    VkFramebuffer myFramebuffer = VK_NULL_HANDLE;

    VkPipeline myPipeline = VK_NULL_HANDLE;

    VkPipelineLayout myPipelineLayout = VK_NULL_HANDLE;

    VkSampler myCompareSampler = VK_NULL_HANDLE;

    VkSampler myDepthReadSampler = VK_NULL_HANDLE;

    glm::mat4 myLightViewProj{ 1.0f };

    float myDepthBiasConstant = 0.0f;
    float myDepthBiasSlope    = 0.0f;

    // Tracked for explicit barriers (render pass initialLayout is ATTACHMENT_OPTIMAL).
    VkImageLayout myDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

namespace Vk_ShadowMapPass {

void Init( Vk_Renderer& aCore );

void Destroy( Vk_Renderer& aCore );

// Ensures shadow depth is shader-readable before DeferredLighting (separate render pass from hybrid resolve).
void CmdBarrierForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aCasterPass, bool aEmitDebugLabels );

}  // namespace Vk_ShadowMapPass

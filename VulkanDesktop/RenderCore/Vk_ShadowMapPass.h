#pragma once

#include "Vk_Types.h"

#include <glm/glm.hpp>

class Vk_Core;
struct Gfx_PassDrawPacket;

struct Vk_ShadowMapState {
    bool myInitialized = false;

    static constexpr uint32_t kMapSize = 2048u;

    Gfx_Texture myDepth{};
    VkRenderPass     myRenderPass     = VK_NULL_HANDLE;
    VkFramebuffer    myFramebuffer    = VK_NULL_HANDLE;
    VkPipeline       myPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout myPipelineLayout = VK_NULL_HANDLE;
    VkSampler        myCompareSampler = VK_NULL_HANDLE;

    glm::mat4 myLightViewProj{ 1.0f };
};

namespace Vk_ShadowMapPass {

void Init( Vk_Core& aCore );
void Destroy( Vk_Core& aCore );

void RecordDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aOpaquePass, uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer,
                 bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels );

}  // namespace Vk_ShadowMapPass

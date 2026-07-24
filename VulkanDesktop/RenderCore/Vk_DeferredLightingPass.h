#pragma once

#include "../Gfx/Gfx_DdgiPass.h"
#include "../Gfx/Gfx_DeferredLightingPass.h"

#include <glm/vec3.hpp>

#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_DeferredLightingState {
    Gfx_DeferredLightingPass::PassState myDeferredGfx{};
    Gfx_DdgiPass::PassState             myDdgiGfx{};

    // DDGI CPU policy / history (probe counts, volume tracking) — not GPU mirrors.
    uint32_t  myDdgiProbeCountX     = 12u;
    uint32_t  myDdgiProbeCountY     = 8u;
    uint32_t  myDdgiProbeCountZ     = 12u;
    uint32_t  myDdgiTotalProbeCount = 0u;
    uint32_t  myDdgiUpdateCursor    = 0u;
    bool      myDdgiAtlasReadable   = false;
    glm::vec3 myDdgiPrevVolumeCenter{ 0.0f };
    glm::vec3 myDdgiPrevVolumeExtents{ 0.0f };
    glm::vec3 myDdgiPrevCameraEye{ 0.0f };
    bool      myDdgiHistoryForceReset = true;

    bool myInitialized = false;
};

// FG v0: fullscreen deferred resolve (G-buffer textures + cluster SSBO from Vk_ClusterBuildPass).
namespace Vk_DeferredLightingPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );
void RecordDdgiProbeUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );

}  // namespace Vk_DeferredLightingPass

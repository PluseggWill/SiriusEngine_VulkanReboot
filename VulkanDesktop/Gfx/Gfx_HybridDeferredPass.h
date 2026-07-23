#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Handles.h"

#include <cstdint>

// Module: Gfx_HybridDeferredPass — GBuffer / hybrid RP Begin/End via Rhi (no vulkan.h).
// Opaque / deferred / transparent draw loops stay outside; FG still orchestrates between Begin/End.

namespace Gfx_HybridDeferredPass {

struct GBufferGpu {
    Rhi_RenderPass  myRenderPass{};
    Rhi_Framebuffer myFramebuffer{};
    uint32_t        myWidth  = 0;
    uint32_t        myHeight = 0;
};

struct HybridGpu {
    Rhi_RenderPass  myRenderPass{};
    Rhi_Framebuffer myFramebuffer{};
    uint32_t        myWidth  = 0;
    uint32_t        myHeight = 0;
};

// Returns true only if BeginRenderPass was issued (caller must End only then).
[[nodiscard]] bool BeginGBuffer( Rhi_CommandList& aCmd, const GBufferGpu& aGpu );
void               EndGBuffer( Rhi_CommandList& aCmd );

[[nodiscard]] bool BeginHybrid( Rhi_CommandList& aCmd, const HybridGpu& aGpu );
void               EndHybrid( Rhi_CommandList& aCmd );

}  // namespace Gfx_HybridDeferredPass

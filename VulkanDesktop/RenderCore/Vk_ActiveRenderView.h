#pragma once

#include "../Gfx/Gfx_ActiveRenderView.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

// RenderCore-owned Vulkan view: API-agnostic Gfx view state plus swapchain viewport/scissor.
struct Vk_ActiveRenderView {
    Gfx_RenderView myView;
    glm::mat4      myCameraView{};
    glm::mat4      myCameraProj{};
    glm::vec3      myCameraEye{ 0.0f };
    VkViewport     myViewport{};
    VkRect2D       myScissor{};
};

inline VkViewport Vk_ToViewport( const VkExtent2D& aExtent, const glm::vec4& aViewportNorm ) {
    const float x      = aViewportNorm.x * static_cast< float >( aExtent.width );
    const float y      = aViewportNorm.y * static_cast< float >( aExtent.height );
    const float width  = std::max( 1.0f, aViewportNorm.z * static_cast< float >( aExtent.width ) );
    const float height = std::max( 1.0f, aViewportNorm.w * static_cast< float >( aExtent.height ) );
    return VkViewport{ x, y, width, height, 0.0f, 1.0f };
}

inline VkRect2D Vk_ToScissor( const VkExtent2D& aExtent, const VkViewport& aViewport ) {
    VkRect2D scissor{};
    scissor.offset.x = std::max( 0, static_cast< int32_t >( aViewport.x ) );
    scissor.offset.y = std::max( 0, static_cast< int32_t >( aViewport.y ) );

    const uint32_t offsetX             = static_cast< uint32_t >( scissor.offset.x );
    const uint32_t offsetY             = static_cast< uint32_t >( scissor.offset.y );
    const uint32_t maxWidthFromOffset  = offsetX < aExtent.width ? ( aExtent.width - offsetX ) : 1u;
    const uint32_t maxHeightFromOffset = offsetY < aExtent.height ? ( aExtent.height - offsetY ) : 1u;
    scissor.extent.width               = std::max( 1u, std::min( maxWidthFromOffset, static_cast< uint32_t >( aViewport.width ) ) );
    scissor.extent.height              = std::max( 1u, std::min( maxHeightFromOffset, static_cast< uint32_t >( aViewport.height ) ) );
    return scissor;
}

inline std::array< Vk_ActiveRenderView, kGfxMaxRenderViews > Vk_ResolveActiveRenderViews( const std::array< Gfx_ActiveRenderView, kGfxMaxRenderViews >& aViews,
                                                                                          uint32_t aViewCount, VkExtent2D aExtent ) {
    std::array< Vk_ActiveRenderView, kGfxMaxRenderViews > outViews{};
    const uint32_t activeViewCount = std::min( aViewCount, kGfxMaxRenderViews );
    for ( uint32_t viewIndex = 0; viewIndex < activeViewCount; ++viewIndex ) {
        outViews[ viewIndex ].myView       = aViews[ viewIndex ].myView;
        outViews[ viewIndex ].myCameraView = aViews[ viewIndex ].myCameraView;
        outViews[ viewIndex ].myCameraProj = aViews[ viewIndex ].myCameraProj;
        outViews[ viewIndex ].myCameraEye  = aViews[ viewIndex ].myCameraEye;
        outViews[ viewIndex ].myViewport   = Vk_ToViewport( aExtent, outViews[ viewIndex ].myView.myViewport );
        outViews[ viewIndex ].myScissor    = Vk_ToScissor( aExtent, outViews[ viewIndex ].myViewport );
    }
    return outViews;
}

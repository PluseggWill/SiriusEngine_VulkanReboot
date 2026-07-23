#include "Vk_FrameCmd.h"

#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

namespace Vk_FrameCmd {

Scope::Scope( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    if ( !aCore.myGfxRhiDevice || aCommandBuffer == VK_NULL_HANDLE ) {
        return;
    }
    myCmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( myCmd, aCommandBuffer );
}

Scope::~Scope() {
    if ( myCmd ) {
        Rhi::CommandListDestroy( myCmd );
    }
}

Scope::Scope( Scope&& aOther ) noexcept : myCmd( std::move( aOther.myCmd ) ) {}

Scope& Scope::operator=( Scope&& aOther ) noexcept {
    if ( this != &aOther ) {
        if ( myCmd ) {
            Rhi::CommandListDestroy( myCmd );
        }
        myCmd = std::move( aOther.myCmd );
    }
    return *this;
}

Rhi_ImageLayout ImageLayoutFromVk( VkImageLayout aLayout ) {
    switch ( aLayout ) {
    case VK_IMAGE_LAYOUT_GENERAL:
        return Rhi_ImageLayout::General;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return Rhi_ImageLayout::ShaderReadOnly;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return Rhi_ImageLayout::ColorAttachment;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return Rhi_ImageLayout::DepthStencilAttachment;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return Rhi_ImageLayout::DepthStencilReadOnly;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return Rhi_ImageLayout::TransferSrc;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return Rhi_ImageLayout::TransferDst;
    case VK_IMAGE_LAYOUT_UNDEFINED:
    default:
        return Rhi_ImageLayout::Undefined;
    }
}

VkImageLayout ImageLayoutToVk( Rhi_ImageLayout aLayout ) {
    switch ( aLayout ) {
    case Rhi_ImageLayout::General:
        return VK_IMAGE_LAYOUT_GENERAL;
    case Rhi_ImageLayout::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case Rhi_ImageLayout::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case Rhi_ImageLayout::DepthStencilAttachment:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case Rhi_ImageLayout::DepthStencilReadOnly:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case Rhi_ImageLayout::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Rhi_ImageLayout::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Rhi_ImageLayout::Undefined:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

}  // namespace Vk_FrameCmd

#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Handles.h"

#include <vulkan/vulkan.h>

class Vk_Renderer;

// Module: Vk_FrameCmd — RAII bind of frame VkCommandBuffer to Rhi_CommandList (RenderCore-only).

namespace Vk_FrameCmd {

struct Scope {
    Rhi_CommandList myCmd{};

    Scope() = default;
    Scope( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );
    ~Scope();

    Scope( const Scope& )            = delete;
    Scope& operator=( const Scope& ) = delete;

    Scope( Scope&& aOther ) noexcept;
    Scope& operator=( Scope&& aOther ) noexcept;

    [[nodiscard]] explicit operator bool() const {
        return static_cast< bool >( myCmd );
    }

    [[nodiscard]] Rhi_CommandList& Get() {
        return myCmd;
    }
};

[[nodiscard]] inline Scope Bind( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    return Scope( aCore, aCommandBuffer );
}

[[nodiscard]] Rhi_ImageLayout ImageLayoutFromVk( VkImageLayout aLayout );
[[nodiscard]] VkImageLayout   ImageLayoutToVk( Rhi_ImageLayout aLayout );

}  // namespace Vk_FrameCmd

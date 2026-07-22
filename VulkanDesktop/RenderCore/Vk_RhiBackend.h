#pragma once

#include "../Rhi/Rhi_Handles.h"

#include <vulkan/vulkan.h>

// Module: Vk_RhiBackend — Vulkan implementation of Rhi_* (RenderCore-only).
// Do not include this header from Gfx/.

class Vk_RhiDevice;

namespace RhiVulkan {

// Non-owning wrap of an existing Vk_RhiDevice (e.g. Vk_Renderer::myRhi). Caller must
// DeviceDestroy the Rhi_Device handle before destroying the underlying Vk_RhiDevice,
// or destroy all command lists first (refcount keeps the shell alive until then).
[[nodiscard]] Rhi_Device DeviceWrap( Vk_RhiDevice& aDevice );

[[nodiscard]] Vk_RhiDevice* DeviceGetVk( const Rhi_Device& aDevice );

void DeviceSetDebugUtils( const Rhi_Device& aDevice, PFN_vkCmdBeginDebugUtilsLabelEXT aBegin, PFN_vkCmdEndDebugUtilsLabelEXT aEnd );

void CommandListBindVk( Rhi_CommandList& aList, VkCommandBuffer aCommandBuffer );

[[nodiscard]] VkCommandBuffer CommandListGetVk( const Rhi_CommandList& aList );

// Non-owning adopt helpers for gradual pass migration (Gfx still uses Rhi types).
[[nodiscard]] Rhi_Pipeline       PipelineAdopt( VkPipeline aPipeline );
[[nodiscard]] Rhi_PipelineLayout PipelineLayoutAdopt( VkPipelineLayout aLayout );
[[nodiscard]] Rhi_DescriptorSet  DescriptorSetAdopt( VkDescriptorSet aSet );
[[nodiscard]] Rhi_RenderPass     RenderPassAdopt( VkRenderPass aRenderPass );
[[nodiscard]] Rhi_Framebuffer    FramebufferAdopt( VkFramebuffer aFramebuffer );
// Registers a non-owned image/view on aDevice so barriers / TextureGetVk* work.
[[nodiscard]] Rhi_Texture TextureAdopt( const Rhi_Device& aDevice, VkImage aImage, VkImageView aView, VkFormat aFormat, uint32_t aMipLevels );
[[nodiscard]] Rhi_Buffer  BufferAdopt( VkBuffer aBuffer );

[[nodiscard]] VkPipeline       PipelineGetVk( Rhi_Pipeline aPipeline );
[[nodiscard]] VkPipelineLayout PipelineLayoutGetVk( Rhi_PipelineLayout aLayout );
[[nodiscard]] VkDescriptorSet  DescriptorSetGetVk( Rhi_DescriptorSet aSet );
[[nodiscard]] VkRenderPass     RenderPassGetVk( Rhi_RenderPass aRenderPass );
[[nodiscard]] VkFramebuffer    FramebufferGetVk( Rhi_Framebuffer aFramebuffer );
[[nodiscard]] VkBuffer         BufferGetVk( const Rhi_Device& aDevice, Rhi_Buffer aBuffer );
[[nodiscard]] VkImage          TextureGetVkImage( const Rhi_Device& aDevice, Rhi_Texture aTexture );
[[nodiscard]] VkImageView      TextureGetVkView( const Rhi_Device& aDevice, Rhi_Texture aTexture );

}  // namespace RhiVulkan

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

[[nodiscard]] VkPipeline            PipelineGetVk( const Rhi_Device& aDevice, Rhi_Pipeline aPipeline );
[[nodiscard]] VkPipelineLayout      PipelineLayoutGetVk( const Rhi_Device& aDevice, Rhi_PipelineLayout aLayout );
[[nodiscard]] VkDescriptorSet       DescriptorSetGetVk( const Rhi_Device& aDevice, Rhi_DescriptorSet aSet );
[[nodiscard]] VkPipeline            PipelineGetVk( Rhi_Pipeline aPipeline );            // adopt-only fallback
[[nodiscard]] VkPipelineLayout      PipelineLayoutGetVk( Rhi_PipelineLayout aLayout );  // adopt-only fallback
[[nodiscard]] VkDescriptorSet       DescriptorSetGetVk( Rhi_DescriptorSet aSet );       // adopt-only fallback
[[nodiscard]] VkRenderPass          RenderPassGetVk( const Rhi_Device& aDevice, Rhi_RenderPass aRenderPass );
[[nodiscard]] VkFramebuffer         FramebufferGetVk( const Rhi_Device& aDevice, Rhi_Framebuffer aFramebuffer );
[[nodiscard]] VkRenderPass          RenderPassGetVk( Rhi_RenderPass aRenderPass );     // adopt-only fallback
[[nodiscard]] VkFramebuffer         FramebufferGetVk( Rhi_Framebuffer aFramebuffer );  // adopt-only fallback
[[nodiscard]] VkBuffer              BufferGetVk( const Rhi_Device& aDevice, Rhi_Buffer aBuffer );
[[nodiscard]] VkImage               TextureGetVkImage( const Rhi_Device& aDevice, Rhi_Texture aTexture );
[[nodiscard]] VkImageView           TextureGetVkView( const Rhi_Device& aDevice, Rhi_Texture aTexture );
[[nodiscard]] VkSampler             SamplerGetVk( const Rhi_Device& aDevice, Rhi_Sampler aSampler );
[[nodiscard]] VkDescriptorSetLayout DescriptorSetLayoutGetVk( const Rhi_Device& aDevice, Rhi_DescriptorSetLayout aLayout );
[[nodiscard]] VkDescriptorPool      DescriptorPoolGetVk( const Rhi_Device& aDevice, Rhi_DescriptorPool aPool );

}  // namespace RhiVulkan

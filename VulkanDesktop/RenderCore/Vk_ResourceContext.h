#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

// Narrow GPU resource factory surface for load-time paths (meshes, textures, tables).
// Vk_Core fills this after device + VMA init; avoids friend access from Vk_ResourceTables.
// TODO(vk-core-peel): extend with CreateBuffer/Image/Copy/Transition/Mipmap from Vk_Core; Util_Loader + Gfx_Mesh take const Vk_ResourceContext&.
struct Vk_ResourceContext {
    VkDevice     myDevice     = VK_NULL_HANDLE;
    VmaAllocator myAllocator  = VK_NULL_HANDLE;

    void Bind( VkDevice aDevice, VmaAllocator aAllocator );

    VkImageView CreateImageView( VkImage anImage, VkFormat aFormat, VkImageAspectFlags anAspect, uint32_t aMipLevel = 1 ) const;
};

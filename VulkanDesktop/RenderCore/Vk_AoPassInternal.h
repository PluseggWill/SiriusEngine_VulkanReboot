#pragma once

#include <vulkan/vulkan.h>

namespace Vk_AoPassDetail {

extern VkImageLayout gAoRawLayout;
extern VkImageLayout gAoHalfLayout;
extern VkImageLayout gBentHalfLayout;
extern VkImageLayout gAoBlurLayout;

void ResetImageLayouts();

}  // namespace Vk_AoPassDetail

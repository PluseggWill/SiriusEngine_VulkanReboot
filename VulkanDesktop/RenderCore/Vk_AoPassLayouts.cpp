#include "Vk_AoPassInternal.h"

namespace Vk_AoPassDetail {

VkImageLayout gAoRawLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gAoHalfLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gBentHalfLayout = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gAoBlurLayout   = VK_IMAGE_LAYOUT_UNDEFINED;

void ResetImageLayouts() {
    gAoRawLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    gAoHalfLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    gBentHalfLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gAoBlurLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace Vk_AoPassDetail

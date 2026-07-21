#include "Vk_PostProcessPassInternal.h"

namespace Vk_PostProcessPassDetail {

VkImageLayout gSceneColorLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gTaaResolvedLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gTaaHistoryLayouts[ 2 ] = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED };
VkImageLayout gBloomPingLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
VkImageLayout gBloomPongLayout        = VK_IMAGE_LAYOUT_UNDEFINED;

void ResetImageLayouts() {
    gSceneColorLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
    gTaaResolvedLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
    gTaaHistoryLayouts[ 0 ] = VK_IMAGE_LAYOUT_UNDEFINED;
    gTaaHistoryLayouts[ 1 ] = VK_IMAGE_LAYOUT_UNDEFINED;
    gBloomPingLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
    gBloomPongLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace Vk_PostProcessPassDetail

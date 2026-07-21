#pragma once

#include "Vk_Types.h"

#include <string>
#include <vulkan/vulkan.h>

class Vk_Renderer;
struct Vk_TextureResource;

namespace Vk_PostProcessPassDetail {

extern VkImageLayout gSceneColorLayout;
extern VkImageLayout gTaaResolvedLayout;
extern VkImageLayout gTaaHistoryLayouts[ 2 ];
extern VkImageLayout gBloomPingLayout;
extern VkImageLayout gBloomPongLayout;

void ResetImageLayouts();

}  // namespace Vk_PostProcessPassDetail

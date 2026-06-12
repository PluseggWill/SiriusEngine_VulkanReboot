#pragma once

#include <array>

#include "../Gfx/Gfx_Vertex.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

VkVertexInputBindingDescription Vk_GetGfxVertexBindingDescription();

std::array< VkVertexInputAttributeDescription, 4 > Vk_GetGfxVertexAttributeDescriptions();

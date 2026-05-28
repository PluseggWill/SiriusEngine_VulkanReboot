#pragma once

#include <cstdint>

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Core;

// Vk_ScenePasses: owns scene pass record orchestration slice (phase-2 #6).
class Vk_ScenePasses {
public:
    static void RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
    static void RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex );
};

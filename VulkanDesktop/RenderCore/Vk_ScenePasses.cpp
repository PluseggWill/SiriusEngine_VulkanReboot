#include "Vk_ScenePasses.h"

#include "Vk_Core.h"

void Vk_ScenePasses::RecordScene( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    aCore.RecordScenePass( aCommandBuffer, anImageIndex );
}

void Vk_ScenePasses::RecordImGui( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t anImageIndex ) {
    aCore.RecordImGuiPass( aCommandBuffer, anImageIndex );
}

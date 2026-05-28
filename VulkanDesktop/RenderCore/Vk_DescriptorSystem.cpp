#include "Vk_DescriptorSystem.h"

#include "Vk_Core.h"

void Vk_DescriptorSystem::InitDeviceLayouts( Vk_Core& aCore ) {
    aCore.CreateDescriptorSetLayout();
    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        aCore.CreateBindlessMaterialSetLayout();
        aCore.CreateBindlessPipelineLayout();
    }
}

void Vk_DescriptorSystem::InitSceneDescriptors( Vk_Core& aCore ) {
    aCore.CreateTextureSampler();
    aCore.CreateDescriptorPool();
    aCore.CreateDescriptorSets();

    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Batch ) {
        aCore.CreateMaterialDescriptorSets();
    }
    else {
        aCore.CreateBindlessDescriptorResources();
    }
}

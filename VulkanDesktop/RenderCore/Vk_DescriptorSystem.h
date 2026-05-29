#pragma once

class Vk_Core;

// Vk_DescriptorSystem: owns descriptor-related orchestration slice (phase-2 #4).
class Vk_DescriptorSystem {
public:
    static void InitDeviceLayouts( Vk_Core& aCore );
    static void InitSceneDescriptors( Vk_Core& aCore );
    static void CreateDescriptorSetLayout( Vk_Core& aCore );
    static void CreateBindlessMaterialSetLayout( Vk_Core& aCore );
    static void CreateBindlessPipelineLayout( Vk_Core& aCore );
    static void CreateBindlessDescriptorResources( Vk_Core& aCore );
    static void CreateDescriptorPool( Vk_Core& aCore );
    static void CreateDescriptorSets( Vk_Core& aCore );
    static void CreateMaterialDescriptorSets( Vk_Core& aCore );
    static void CreateTextureSampler( Vk_Core& aCore );

    // One-shot log: pipeline set order + bindings vs shaders (descriptor-layout-verify).
    static void LogLayoutContract( const Vk_Core& aCore );
};

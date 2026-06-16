#pragma once

class Vk_Renderer;

// Vk_DescriptorSystem: owns descriptor-related orchestration slice (phase-2 #4).
class Vk_DescriptorSystem {
public:
    static void InitDeviceLayouts( Vk_Renderer& aCore );
    static void InitSceneDescriptors( Vk_Renderer& aCore );
    static void CreateDescriptorSetLayout( Vk_Renderer& aCore );
    static void CreateBindlessMaterialSetLayout( Vk_Renderer& aCore );
    static void CreateBindlessPipelineLayout( Vk_Renderer& aCore );
    static void CreateBindlessDescriptorResources( Vk_Renderer& aCore );
    static void CreateDescriptorPool( Vk_Renderer& aCore );
    static void CreateDescriptorSets( Vk_Renderer& aCore );
    static void CreateMaterialDescriptorSets( Vk_Renderer& aCore );
    static void CreateTextureSampler( Vk_Renderer& aCore );
    static void EnsureBindlessDefaultTexture( Vk_Renderer& aCore );

    // One-shot log: pipeline set order + bindings vs shaders (descriptor-layout-verify).
    static void LogLayoutContract( const Vk_Renderer& aCore );
};

#pragma once

class Vk_Core;

// Vk_DescriptorSystem: owns descriptor-related orchestration slice (phase-2 #4).
class Vk_DescriptorSystem {
public:
    static void InitDeviceLayouts( Vk_Core& aCore );
    static void InitSceneDescriptors( Vk_Core& aCore );
};

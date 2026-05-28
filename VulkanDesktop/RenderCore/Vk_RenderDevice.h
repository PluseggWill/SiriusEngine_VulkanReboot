#pragma once

class Vk_Core;

// Vk_RenderDevice: orchestrates Vulkan device bootstrap slice (phase-2 #2).
// Scope: instance/surface/physical-device/queue-family/logical-device/command-pool/allocator.
class Vk_RenderDevice {
public:
    static void Init( Vk_Core& aCore );
};

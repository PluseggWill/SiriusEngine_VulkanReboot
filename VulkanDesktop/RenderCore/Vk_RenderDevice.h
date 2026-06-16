#pragma once

class Vk_Renderer;
class App_PlatformHost;

// Vk_RenderDevice: orchestrates Vulkan device bootstrap slice (phase-2 #2).
// Scope: instance/surface/physical-device/queue-family/logical-device/command-pool/allocator.
class Vk_RenderDevice {
public:
    static void Init( Vk_Renderer& aCore, App_PlatformHost& aPlatformHost );
};

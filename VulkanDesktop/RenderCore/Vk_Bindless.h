#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

// S1 bindless v0: probe + render path selection. Policy: EngineArchitecture.md §6; decision: Docs/shader-bindless-policy_Plan.md

enum class Vk_RenderMaterialPath : uint8_t {
    Batch    = 0,
    Bindless = 1,
};

struct Vk_BindlessCapabilities {
    bool myDescriptorIndexingExtension = false;
    bool myRuntimeDescriptorArray      = false;
    bool myNonUniformIndexing          = false;
};

// Query physical device; optionally append VK_EXT_descriptor_indexing to device extensions.
Vk_BindlessCapabilities Vk_ProbeBindlessCapabilities( VkPhysicalDevice aPhysicalDevice, std::vector< const char* >& aDeviceExtensions );

// After device creation: pick bindless vs batch (env FORCE_MATERIAL_BATCH=1 forces batch).
Vk_RenderMaterialPath Vk_SelectRenderMaterialPath( const Vk_BindlessCapabilities& aCaps );

const char* Vk_RenderMaterialPathName( Vk_RenderMaterialPath aPath );

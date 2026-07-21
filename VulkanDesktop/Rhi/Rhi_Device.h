#pragma once

#include "Rhi_Enums.h"
#include "Rhi_Handles.h"

#include <cstddef>
#include <cstdint>

// Module: Rhi_Device — opaque device lifecycle (Gfx-facing).

namespace Rhi {

// Headless Vulkan instance for tests/probes (no logical device / no WSI).
[[nodiscard]] Rhi_Device DeviceCreateHeadless( bool aEnableValidationLayers );

void DeviceDestroy( Rhi_Device& aDevice );

[[nodiscard]] bool DeviceIsValid( const Rhi_Device& aDevice );

// True when a logical device is available (wrap path or future full create).
[[nodiscard]] bool DeviceHasLogicalDevice( const Rhi_Device& aDevice );

[[nodiscard]] Rhi_CommandList DeviceCreateCommandList( Rhi_Device& aDevice );

void CommandListDestroy( Rhi_CommandList& aList );

// --- Resource factory (requires logical device; returns empty if unavailable) ---

struct BufferDesc {
    uint64_t        mySizeBytes = 0;
    Rhi_BufferUsage myUsage     = Rhi_BufferUsage::Uniform;
    Rhi_MemoryUsage myMemory    = Rhi_MemoryUsage::CpuToGpu;
};

[[nodiscard]] Rhi_Buffer DeviceCreateBuffer( Rhi_Device& aDevice, const BufferDesc& aDesc );
void                     DeviceDestroyBuffer( Rhi_Device& aDevice, Rhi_Buffer& aBuffer );

struct TextureDesc {
    uint32_t        myWidth     = 1;
    uint32_t        myHeight    = 1;
    uint32_t        myMipLevels = 1;
    Rhi_Format      myFormat    = Rhi_Format::RGBA8_Unorm;
    Rhi_ImageUsage  myUsage     = Rhi_ImageUsage::Sampled;
    Rhi_MemoryUsage myMemory    = Rhi_MemoryUsage::GpuOnly;
};

[[nodiscard]] Rhi_Texture DeviceCreateTexture( Rhi_Device& aDevice, const TextureDesc& aDesc );
void                      DeviceDestroyTexture( Rhi_Device& aDevice, Rhi_Texture& aTexture );

[[nodiscard]] Rhi_ShaderModule DeviceCreateShaderModule( Rhi_Device& aDevice, const void* aSpirvCode, size_t aSpirvBytes );
void                           DeviceDestroyShaderModule( Rhi_Device& aDevice, Rhi_ShaderModule& aModule );

}  // namespace Rhi

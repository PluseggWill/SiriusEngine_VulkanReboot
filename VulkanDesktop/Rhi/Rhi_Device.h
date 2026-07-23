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

// --- Samplers / descriptors / compute pipelines (requires logical device) ---

struct SamplerDesc {
    Rhi_Filter      myMagFilter     = Rhi_Filter::Nearest;
    Rhi_Filter      myMinFilter     = Rhi_Filter::Nearest;
    Rhi_MipmapMode  myMipmapMode    = Rhi_MipmapMode::Nearest;
    Rhi_AddressMode myAddressU      = Rhi_AddressMode::ClampToEdge;
    Rhi_AddressMode myAddressV      = Rhi_AddressMode::ClampToEdge;
    Rhi_AddressMode myAddressW      = Rhi_AddressMode::ClampToEdge;
    float           myMaxLod        = 1000.0f;
    bool            myCompareEnable = false;
};

[[nodiscard]] Rhi_Sampler DeviceCreateSampler( Rhi_Device& aDevice, const SamplerDesc& aDesc );
void                      DeviceDestroySampler( Rhi_Device& aDevice, Rhi_Sampler& aSampler );

struct DescriptorSetLayoutBinding {
    uint32_t           myBinding = 0;
    Rhi_DescriptorType myType    = Rhi_DescriptorType::CombinedImageSampler;
    uint32_t           myCount   = 1;
    Rhi_ShaderStage    myStages  = Rhi_ShaderStage::Compute;
};

struct DescriptorSetLayoutDesc {
    const DescriptorSetLayoutBinding* myBindings     = nullptr;
    uint32_t                          myBindingCount = 0;
};

[[nodiscard]] Rhi_DescriptorSetLayout DeviceCreateDescriptorSetLayout( Rhi_Device& aDevice, const DescriptorSetLayoutDesc& aDesc );
void                                  DeviceDestroyDescriptorSetLayout( Rhi_Device& aDevice, Rhi_DescriptorSetLayout& aLayout );

struct DescriptorPoolSize {
    Rhi_DescriptorType myType  = Rhi_DescriptorType::CombinedImageSampler;
    uint32_t           myCount = 0;
};

struct DescriptorPoolDesc {
    uint32_t                  myMaxSets       = 0;
    const DescriptorPoolSize* myPoolSizes     = nullptr;
    uint32_t                  myPoolSizeCount = 0;
};

[[nodiscard]] Rhi_DescriptorPool DeviceCreateDescriptorPool( Rhi_Device& aDevice, const DescriptorPoolDesc& aDesc );
void                             DeviceDestroyDescriptorPool( Rhi_Device& aDevice, Rhi_DescriptorPool& aPool );

// Allocated sets are freed when the pool is destroyed.
[[nodiscard]] Rhi_DescriptorSet DeviceAllocateDescriptorSet( Rhi_Device& aDevice, Rhi_DescriptorPool aPool, Rhi_DescriptorSetLayout aLayout );

struct PushConstantRangeDesc {
    Rhi_ShaderStage myStages      = Rhi_ShaderStage::Compute;
    uint32_t        myOffsetBytes = 0;
    uint32_t        mySizeBytes   = 0;
};

struct PipelineLayoutDesc {
    const Rhi_DescriptorSetLayout* mySetLayouts     = nullptr;
    uint32_t                       mySetLayoutCount = 0;
    const PushConstantRangeDesc*   myPushRanges     = nullptr;
    uint32_t                       myPushRangeCount = 0;
};

[[nodiscard]] Rhi_PipelineLayout DeviceCreatePipelineLayout( Rhi_Device& aDevice, const PipelineLayoutDesc& aDesc );
void                             DeviceDestroyPipelineLayout( Rhi_Device& aDevice, Rhi_PipelineLayout& aLayout );

struct ComputePipelineDesc {
    Rhi_ShaderModule   myShader{};
    Rhi_PipelineLayout myLayout{};
    const char*        myEntry = "main";
};

[[nodiscard]] Rhi_Pipeline DeviceCreateComputePipeline( Rhi_Device& aDevice, const ComputePipelineDesc& aDesc );
void                       DeviceDestroyPipeline( Rhi_Device& aDevice, Rhi_Pipeline& aPipeline );

// --- Graphics create (RP / FB / graphics PSO) ---

struct AttachmentDesc {
    Rhi_Format            myFormat        = Rhi_Format::RGBA8_Unorm;
    Rhi_AttachmentLoadOp  myLoadOp        = Rhi_AttachmentLoadOp::Clear;
    Rhi_AttachmentStoreOp myStoreOp       = Rhi_AttachmentStoreOp::Store;
    Rhi_ImageLayout       myInitialLayout = Rhi_ImageLayout::Undefined;
    Rhi_ImageLayout       myFinalLayout   = Rhi_ImageLayout::ColorAttachment;
    uint32_t              mySampleCount   = 1;
};

struct RenderPassDesc {
    const AttachmentDesc* myAttachments                 = nullptr;
    uint32_t              myAttachmentCount             = 0;
    const uint32_t*       myColorAttachmentIndices      = nullptr;
    uint32_t              myColorAttachmentCount        = 0;
    bool                  myHasDepthStencil             = false;
    uint32_t              myDepthStencilAttachmentIndex = 0;
};

[[nodiscard]] Rhi_RenderPass DeviceCreateRenderPass( Rhi_Device& aDevice, const RenderPassDesc& aDesc );
void                         DeviceDestroyRenderPass( Rhi_Device& aDevice, Rhi_RenderPass& aRenderPass );

struct FramebufferDesc {
    Rhi_RenderPass     myRenderPass{};
    const Rhi_Texture* myAttachments     = nullptr;
    uint32_t           myAttachmentCount = 0;
    uint32_t           myWidth           = 0;
    uint32_t           myHeight          = 0;
};

[[nodiscard]] Rhi_Framebuffer DeviceCreateFramebuffer( Rhi_Device& aDevice, const FramebufferDesc& aDesc );
void                          DeviceDestroyFramebuffer( Rhi_Device& aDevice, Rhi_Framebuffer& aFramebuffer );

struct GraphicsPipelineDesc {
    Rhi_ShaderModule      myVertexShader{};
    Rhi_ShaderModule      myFragmentShader{};
    Rhi_PipelineLayout    myLayout{};
    Rhi_RenderPass        myRenderPass{};
    uint32_t              mySubpass                = 0;
    uint32_t              myColorAttachmentCount   = 1;
    uint32_t              mySampleCount            = 1;
    Rhi_CullMode          myCullMode               = Rhi_CullMode::None;
    Rhi_PrimitiveTopology myTopology               = Rhi_PrimitiveTopology::TriangleList;
    bool                  myDepthTestEnable        = false;
    bool                  myDepthWriteEnable       = false;
    Rhi_CompareOp         myDepthCompareOp         = Rhi_CompareOp::LessOrEqual;
    bool                  myBlendEnable            = false;
    bool                  myDynamicViewportScissor = true;
    bool                  myDynamicDepthBias       = false;
};

[[nodiscard]] Rhi_Pipeline DeviceCreateGraphicsPipeline( Rhi_Device& aDevice, const GraphicsPipelineDesc& aDesc );

// Single-mip view of an existing texture (owns the view; does not own the image).
[[nodiscard]] Rhi_Texture DeviceCreateTextureMipView( Rhi_Device& aDevice, Rhi_Texture aParent, uint32_t aBaseMip );

struct DescriptorImageWrite {
    Rhi_DescriptorSet  mySet{};
    uint32_t           myBinding = 0;
    Rhi_DescriptorType myType    = Rhi_DescriptorType::CombinedImageSampler;
    Rhi_Sampler        mySampler{};
    Rhi_Texture        myTexture{};
    Rhi_ImageLayout    myLayout = Rhi_ImageLayout::General;
};

void DeviceUpdateDescriptorImages( Rhi_Device& aDevice, const DescriptorImageWrite* aWrites, uint32_t aWriteCount );

struct DescriptorBufferWrite {
    Rhi_DescriptorSet  mySet{};
    uint32_t           myBinding = 0;
    Rhi_DescriptorType myType    = Rhi_DescriptorType::StorageBuffer;
    Rhi_Buffer         myBuffer{};
    uint64_t           myOffsetBytes = 0;
    uint64_t           myRangeBytes  = 0;  // 0 = VK_WHOLE_SIZE
};

void DeviceUpdateDescriptorBuffers( Rhi_Device& aDevice, const DescriptorBufferWrite* aWrites, uint32_t aWriteCount );

[[nodiscard]] void* DeviceMapBuffer( Rhi_Device& aDevice, Rhi_Buffer aBuffer );
void                DeviceUnmapBuffer( Rhi_Device& aDevice, Rhi_Buffer aBuffer );

}  // namespace Rhi

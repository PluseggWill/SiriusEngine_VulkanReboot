#include "Vk_RhiBackend.h"

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "Vk_RhiDevice.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

namespace {

struct BufferRecord {
    Vk_AllocatedBuffer myAlloc{};
    bool               myOwned = true;
};

struct TextureRecord {
    Vk_AllocatedImage myAlloc{};
    VkImageView       myView      = VK_NULL_HANDLE;
    VkFormat          myFormat    = VK_FORMAT_UNDEFINED;
    uint32_t          myMipLevels = 1;
    bool              myOwned     = true;  // owns image (+ view when owned create)
    bool              myOwnsView  = true;  // destroy view on DeviceDestroyTexture
};

struct DeviceImpl;

struct CommandListImpl {
    DeviceImpl*     myDevice = nullptr;
    VkCommandBuffer myCmd    = VK_NULL_HANDLE;
};

struct DeviceImpl {
    Vk_RhiDevice*                                                          myVk       = nullptr;
    bool                                                                   myOwnsVk   = false;
    int                                                                    myRefCount = 1;
    std::vector< CommandListImpl* >                                        myCommandLists;
    PFN_vkCmdBeginDebugUtilsLabelEXT                                       myCmdBeginLabel  = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT                                         myCmdEndLabel    = nullptr;
    uint64_t                                                               myNextResourceId = 1;
    std::unordered_map< uint64_t, BufferRecord >                           myBuffers;
    std::unordered_map< uint64_t, TextureRecord >                          myTextures;
    std::unordered_map< uint64_t, VkShaderModule >                         myShaders;
    std::unordered_map< uint64_t, VkSampler >                              mySamplers;
    std::unordered_map< uint64_t, VkDescriptorSetLayout >                  mySetLayouts;
    std::unordered_map< uint64_t, VkDescriptorPool >                       myPools;
    std::unordered_map< uint64_t, VkPipelineLayout >                       myPipelineLayouts;
    std::unordered_map< uint64_t, VkPipeline >                             myPipelines;
    std::unordered_map< uint64_t, std::pair< VkDescriptorSet, uint64_t > > myDescriptorSets;  // set, poolId
};

DeviceImpl* AsDevice( const Rhi_Device& aDevice ) {
    return static_cast< DeviceImpl* >( aDevice.myImpl );
}

CommandListImpl* AsCmd( const Rhi_CommandList& aList ) {
    return static_cast< CommandListImpl* >( aList.myImpl );
}

void RetainDevice( DeviceImpl* aImpl ) {
    if ( aImpl != nullptr ) {
        ++aImpl->myRefCount;
    }
}

void DestroyOwnedResources( DeviceImpl* aImpl ) {
    if ( aImpl == nullptr || aImpl->myVk == nullptr ) {
        return;
    }
    VkDevice device = aImpl->myVk->myDeviceCtx.myDevice;
    if ( device == VK_NULL_HANDLE ) {
        aImpl->myBuffers.clear();
        aImpl->myTextures.clear();
        aImpl->myShaders.clear();
        aImpl->mySamplers.clear();
        aImpl->mySetLayouts.clear();
        aImpl->myPools.clear();
        aImpl->myPipelineLayouts.clear();
        aImpl->myPipelines.clear();
        aImpl->myDescriptorSets.clear();
        return;
    }

    for ( auto& [ id, pipeline ] : aImpl->myPipelines ) {
        ( void )id;
        if ( pipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( device, pipeline, nullptr );
        }
    }
    aImpl->myPipelines.clear();

    for ( auto& [ id, layout ] : aImpl->myPipelineLayouts ) {
        ( void )id;
        if ( layout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, layout, nullptr );
        }
    }
    aImpl->myPipelineLayouts.clear();

    aImpl->myDescriptorSets.clear();  // pool owns set memory

    for ( auto& [ id, pool ] : aImpl->myPools ) {
        ( void )id;
        if ( pool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, pool, nullptr );
        }
    }
    aImpl->myPools.clear();

    for ( auto& [ id, layout ] : aImpl->mySetLayouts ) {
        ( void )id;
        if ( layout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, layout, nullptr );
        }
    }
    aImpl->mySetLayouts.clear();

    for ( auto& [ id, sampler ] : aImpl->mySamplers ) {
        ( void )id;
        if ( sampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, sampler, nullptr );
        }
    }
    aImpl->mySamplers.clear();

    for ( auto& [ id, shader ] : aImpl->myShaders ) {
        ( void )id;
        if ( shader != VK_NULL_HANDLE ) {
            vkDestroyShaderModule( device, shader, nullptr );
        }
    }
    aImpl->myShaders.clear();

    for ( auto& [ id, tex ] : aImpl->myTextures ) {
        ( void )id;
        if ( tex.myView != VK_NULL_HANDLE && tex.myOwnsView ) {
            vkDestroyImageView( device, tex.myView, nullptr );
        }
        if ( tex.myOwned && tex.myAlloc.myImage != VK_NULL_HANDLE && aImpl->myVk->myDeviceCtx.myAllocator != nullptr ) {
            vmaDestroyImage( aImpl->myVk->myDeviceCtx.myAllocator, tex.myAlloc.myImage, tex.myAlloc.myAllocation );
        }
    }
    aImpl->myTextures.clear();

    for ( auto& [ id, buf ] : aImpl->myBuffers ) {
        ( void )id;
        if ( buf.myOwned && buf.myAlloc.myBuffer != VK_NULL_HANDLE && aImpl->myVk->myDeviceCtx.myAllocator != nullptr ) {
            vmaDestroyBuffer( aImpl->myVk->myDeviceCtx.myAllocator, buf.myAlloc.myBuffer, buf.myAlloc.myAllocation );
        }
    }
    aImpl->myBuffers.clear();
}

void ReleaseDevice( DeviceImpl* aImpl ) {
    if ( aImpl == nullptr ) {
        return;
    }
    if ( --aImpl->myRefCount > 0 ) {
        return;
    }
    // Live command lists must have dropped their refs already.
    aImpl->myCommandLists.clear();
    DestroyOwnedResources( aImpl );
    if ( aImpl->myOwnsVk && aImpl->myVk != nullptr ) {
        if ( aImpl->myVk->IsInstanceCreated() ) {
            vkDestroyInstance( aImpl->myVk->myDeviceCtx.myInstance, nullptr );
            aImpl->myVk->myDeviceCtx.myInstance = VK_NULL_HANDLE;
        }
        delete aImpl->myVk;
        aImpl->myVk = nullptr;
    }
    delete aImpl;
}

void UnregisterCommandList( DeviceImpl* aDevice, CommandListImpl* aList ) {
    if ( aDevice == nullptr || aList == nullptr ) {
        return;
    }
    auto& lists = aDevice->myCommandLists;
    for ( size_t i = 0; i < lists.size(); ++i ) {
        if ( lists[ i ] == aList ) {
            lists[ i ] = lists.back();
            lists.pop_back();
            break;
        }
    }
}

VkBufferUsageFlags ToVkBufferUsage( Rhi_BufferUsage aUsage ) {
    VkBufferUsageFlags flags = 0;
    const auto         bits  = static_cast< uint32_t >( aUsage );
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::TransferSrc ) ) {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::TransferDst ) ) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::Uniform ) ) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::Storage ) ) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::Vertex ) ) {
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_BufferUsage::Index ) ) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    return flags;
}

VkImageUsageFlags ToVkImageUsage( Rhi_ImageUsage aUsage ) {
    VkImageUsageFlags flags = 0;
    const auto        bits  = static_cast< uint32_t >( aUsage );
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::TransferSrc ) ) {
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::TransferDst ) ) {
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::Sampled ) ) {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::Storage ) ) {
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::ColorAttachment ) ) {
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ImageUsage::DepthStencilAttachment ) ) {
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return flags;
}

VmaMemoryUsage ToVmaMemory( Rhi_MemoryUsage aUsage ) {
    switch ( aUsage ) {
    case Rhi_MemoryUsage::CpuToGpu:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case Rhi_MemoryUsage::GpuToCpu:
        return VMA_MEMORY_USAGE_GPU_TO_CPU;
    case Rhi_MemoryUsage::GpuOnly:
    default:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    }
}

VkFormat ToVkFormat( Rhi_Format aFormat ) {
    switch ( aFormat ) {
    case Rhi_Format::R8_Unorm:
        return VK_FORMAT_R8_UNORM;
    case Rhi_Format::R16_Sfloat:
        return VK_FORMAT_R16_SFLOAT;
    case Rhi_Format::R32_Sfloat:
        return VK_FORMAT_R32_SFLOAT;
    case Rhi_Format::RG16_Sfloat:
        return VK_FORMAT_R16G16_SFLOAT;
    case Rhi_Format::RGBA8_Unorm:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Rhi_Format::RGBA16_Sfloat:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Rhi_Format::D32_Sfloat:
        return VK_FORMAT_D32_SFLOAT;
    case Rhi_Format::Undefined:
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

VkImageLayout ToVkLayout( Rhi_ImageLayout aLayout ) {
    switch ( aLayout ) {
    case Rhi_ImageLayout::General:
        return VK_IMAGE_LAYOUT_GENERAL;
    case Rhi_ImageLayout::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case Rhi_ImageLayout::DepthStencilAttachment:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case Rhi_ImageLayout::DepthStencilReadOnly:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case Rhi_ImageLayout::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case Rhi_ImageLayout::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case Rhi_ImageLayout::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case Rhi_ImageLayout::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case Rhi_ImageLayout::Undefined:
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkPipelineStageFlags ToVkStage( Rhi_PipelineStage aStage ) {
    VkPipelineStageFlags flags = 0;
    const auto           bits  = static_cast< uint32_t >( aStage );
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::TopOfPipe ) ) {
        flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::DrawIndirect ) ) {
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::VertexShader ) ) {
        flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::FragmentShader ) ) {
        flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::ComputeShader ) ) {
        flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::ColorAttachment ) ) {
        flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::Transfer ) ) {
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::BottomOfPipe ) ) {
        flags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::AllCommands ) ) {
        flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::Host ) ) {
        flags |= VK_PIPELINE_STAGE_HOST_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::EarlyFragmentTests ) ) {
        flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_PipelineStage::LateFragmentTests ) ) {
        flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }
    return flags == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : flags;
}

VkAccessFlags ToVkAccess( Rhi_Access aAccess ) {
    VkAccessFlags flags = 0;
    const auto    bits  = static_cast< uint32_t >( aAccess );
    if ( bits & static_cast< uint32_t >( Rhi_Access::ShaderRead ) ) {
        flags |= VK_ACCESS_SHADER_READ_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::ShaderWrite ) ) {
        flags |= VK_ACCESS_SHADER_WRITE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::ColorAttachmentRW ) ) {
        flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::DepthStencilRW ) ) {
        flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::TransferRead ) ) {
        flags |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::TransferWrite ) ) {
        flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_Access::HostWrite ) ) {
        flags |= VK_ACCESS_HOST_WRITE_BIT;
    }
    return flags;
}

VkShaderStageFlags ToVkShaderStages( Rhi_ShaderStage aStages ) {
    VkShaderStageFlags flags = 0;
    const auto         bits  = static_cast< uint32_t >( aStages );
    if ( bits & static_cast< uint32_t >( Rhi_ShaderStage::Vertex ) ) {
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ShaderStage::Fragment ) ) {
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if ( bits & static_cast< uint32_t >( Rhi_ShaderStage::Compute ) ) {
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    return flags;
}

VkPipelineBindPoint ToVkBindPoint( Rhi_PipelineBindPoint aBindPoint ) {
    return aBindPoint == Rhi_PipelineBindPoint::Compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
}

bool HasLogicalDevice( const DeviceImpl* aImpl ) {
    return aImpl != nullptr && aImpl->myVk != nullptr && aImpl->myVk->myDeviceCtx.myDevice != VK_NULL_HANDLE;
}

VkPipeline ResolvePipeline( DeviceImpl* aDevice, Rhi_Pipeline aPipeline ) {
    if ( aDevice != nullptr ) {
        auto it = aDevice->myPipelines.find( aPipeline.myId );
        if ( it != aDevice->myPipelines.end() ) {
            return it->second;
        }
    }
    return reinterpret_cast< VkPipeline >( static_cast< uintptr_t >( aPipeline.myId ) );
}

VkPipelineLayout ResolvePipelineLayout( DeviceImpl* aDevice, Rhi_PipelineLayout aLayout ) {
    if ( aDevice != nullptr ) {
        auto it = aDevice->myPipelineLayouts.find( aLayout.myId );
        if ( it != aDevice->myPipelineLayouts.end() ) {
            return it->second;
        }
    }
    return reinterpret_cast< VkPipelineLayout >( static_cast< uintptr_t >( aLayout.myId ) );
}

VkDescriptorSet ResolveDescriptorSet( DeviceImpl* aDevice, Rhi_DescriptorSet aSet ) {
    if ( aDevice != nullptr ) {
        auto it = aDevice->myDescriptorSets.find( aSet.myId );
        if ( it != aDevice->myDescriptorSets.end() ) {
            return it->second.first;
        }
    }
    return reinterpret_cast< VkDescriptorSet >( static_cast< uintptr_t >( aSet.myId ) );
}

VkRenderPass ResolveRenderPass( DeviceImpl* aDevice, Rhi_RenderPass aPass ) {
    ( void )aDevice;
    return reinterpret_cast< VkRenderPass >( static_cast< uintptr_t >( aPass.myId ) );
}

VkFramebuffer ResolveFramebuffer( DeviceImpl* aDevice, Rhi_Framebuffer aFb ) {
    ( void )aDevice;
    return reinterpret_cast< VkFramebuffer >( static_cast< uintptr_t >( aFb.myId ) );
}

Rhi_Device MakeDeviceHandle( DeviceImpl* aImpl ) {
    Rhi_Device device;
    device.myImpl = aImpl;
    return device;
}

}  // namespace

namespace Rhi {

Rhi_Device DeviceCreateHeadless( bool aEnableValidationLayers ) {
    auto* vk = new Vk_RhiDevice();
    vk->SetEnableValidationLayers( aEnableValidationLayers, {} );
    if ( !vk->CreateInstanceHeadless() ) {
        delete vk;
        return {};
    }
    auto* impl       = new DeviceImpl();
    impl->myVk       = vk;
    impl->myOwnsVk   = true;
    impl->myRefCount = 1;
    return MakeDeviceHandle( impl );
}

void DeviceDestroy( Rhi_Device& aDevice ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return;
    }
    aDevice.myImpl = nullptr;
    ReleaseDevice( impl );
}

bool DeviceIsValid( const Rhi_Device& aDevice ) {
    const DeviceImpl* impl = AsDevice( aDevice );
    return impl != nullptr && impl->myVk != nullptr && impl->myVk->IsInstanceCreated();
}

bool DeviceHasLogicalDevice( const Rhi_Device& aDevice ) {
    return HasLogicalDevice( AsDevice( aDevice ) );
}

Rhi_CommandList DeviceCreateCommandList( Rhi_Device& aDevice ) {
    DeviceImpl* deviceImpl = AsDevice( aDevice );
    if ( deviceImpl == nullptr ) {
        return {};
    }
    auto* cmdImpl     = new CommandListImpl();
    cmdImpl->myDevice = deviceImpl;
    RetainDevice( deviceImpl );
    deviceImpl->myCommandLists.push_back( cmdImpl );
    Rhi_CommandList list;
    list.myImpl = cmdImpl;
    return list;
}

void CommandListDestroy( Rhi_CommandList& aList ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr ) {
        return;
    }
    DeviceImpl* device = impl->myDevice;
    UnregisterCommandList( device, impl );
    delete impl;
    aList.myImpl = nullptr;
    ReleaseDevice( device );
}

bool CommandListIsValid( const Rhi_CommandList& aList ) {
    return AsCmd( aList ) != nullptr;
}

bool CommandListIsRecordingReady( const Rhi_CommandList& aList ) {
    const CommandListImpl* impl = AsCmd( aList );
    return impl != nullptr && impl->myCmd != VK_NULL_HANDLE;
}

void CommandListBeginDebugLabel( Rhi_CommandList& aList, const char* aName ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || impl->myDevice == nullptr || impl->myDevice->myCmdBeginLabel == nullptr || aName == nullptr ) {
        return;
    }
    VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.pLabelName = aName;
    impl->myDevice->myCmdBeginLabel( impl->myCmd, &label );
}

void CommandListEndDebugLabel( Rhi_CommandList& aList ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || impl->myDevice == nullptr || impl->myDevice->myCmdEndLabel == nullptr ) {
        return;
    }
    impl->myDevice->myCmdEndLabel( impl->myCmd );
}

void CommandListPipelineBarrier( Rhi_CommandList& aList, const ImageBarrier* aBarriers, uint32_t aBarrierCount ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || aBarriers == nullptr || aBarrierCount == 0 || !HasLogicalDevice( impl->myDevice ) ) {
        return;
    }

    std::vector< VkImageMemoryBarrier > vkBarriers;
    vkBarriers.reserve( aBarrierCount );
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    for ( uint32_t i = 0; i < aBarrierCount; ++i ) {
        const ImageBarrier& b  = aBarriers[ i ];
        auto                it = impl->myDevice->myTextures.find( b.myTexture.myId );
        if ( it == impl->myDevice->myTextures.end() || it->second.myAlloc.myImage == VK_NULL_HANDLE ) {
            continue;
        }
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout                       = ToVkLayout( b.myOldLayout );
        barrier.newLayout                       = ToVkLayout( b.myNewLayout );
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = it->second.myAlloc.myImage;
        barrier.subresourceRange.aspectMask     = ( it->second.myFormat == VK_FORMAT_D32_SFLOAT ) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = b.myBaseMip;
        barrier.subresourceRange.levelCount     = b.myMipCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = ToVkAccess( b.mySrcAccess );
        barrier.dstAccessMask                   = ToVkAccess( b.myDstAccess );
        srcStage |= ToVkStage( b.mySrcStage );
        dstStage |= ToVkStage( b.myDstStage );
        vkBarriers.push_back( barrier );
    }

    if ( vkBarriers.empty() ) {
        return;
    }
    vkCmdPipelineBarrier( impl->myCmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, static_cast< uint32_t >( vkBarriers.size() ), vkBarriers.data() );
}

void CommandListPipelineBarrier( Rhi_CommandList& aList, const BufferBarrier* aBarriers, uint32_t aBarrierCount ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || aBarriers == nullptr || aBarrierCount == 0 || impl->myDevice == nullptr ) {
        return;
    }

    std::vector< VkBufferMemoryBarrier > vkBarriers;
    vkBarriers.reserve( aBarrierCount );
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;

    for ( uint32_t i = 0; i < aBarrierCount; ++i ) {
        const BufferBarrier& b  = aBarriers[ i ];
        auto                 it = impl->myDevice->myBuffers.find( b.myBuffer.myId );
        const VkBuffer       resolved =
            ( it != impl->myDevice->myBuffers.end() ) ? it->second.myAlloc.myBuffer : reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( b.myBuffer.myId ) );
        if ( resolved == VK_NULL_HANDLE ) {
            continue;
        }
        VkBufferMemoryBarrier barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        barrier.srcAccessMask       = ToVkAccess( b.mySrcAccess );
        barrier.dstAccessMask       = ToVkAccess( b.myDstAccess );
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer              = resolved;
        barrier.offset              = 0;
        barrier.size                = VK_WHOLE_SIZE;
        srcStage |= ToVkStage( b.mySrcStage );
        dstStage |= ToVkStage( b.myDstStage );
        vkBarriers.push_back( barrier );
    }

    if ( vkBarriers.empty() ) {
        return;
    }
    vkCmdPipelineBarrier( impl->myCmd, srcStage, dstStage, 0, 0, nullptr, static_cast< uint32_t >( vkBarriers.size() ), vkBarriers.data(), 0, nullptr );
}

void CommandListMemoryBarrier( Rhi_CommandList& aList, const MemoryBarrierDesc& aBarrier ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = ToVkAccess( aBarrier.mySrcAccess );
    barrier.dstAccessMask = ToVkAccess( aBarrier.myDstAccess );
    vkCmdPipelineBarrier( impl->myCmd, ToVkStage( aBarrier.mySrcStage ), ToVkStage( aBarrier.myDstStage ), 0, 1, &barrier, 0, nullptr, 0, nullptr );
}

void CommandListBindPipeline( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_Pipeline aPipeline ) {
    CommandListImpl* impl = AsCmd( aList );
    VkPipeline       pipe = ResolvePipeline( impl != nullptr ? impl->myDevice : nullptr, aPipeline );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || pipe == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdBindPipeline( impl->myCmd, ToVkBindPoint( aBindPoint ), pipe );
}

void CommandListBindDescriptorSet( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_PipelineLayout aLayout, uint32_t aSetIndex, Rhi_DescriptorSet aSet,
                                   const uint32_t* aDynamicOffsets, uint32_t aDynamicOffsetCount ) {
    CommandListImpl* impl   = AsCmd( aList );
    DeviceImpl*      device = impl != nullptr ? impl->myDevice : nullptr;
    VkPipelineLayout layout = ResolvePipelineLayout( device, aLayout );
    VkDescriptorSet  set    = ResolveDescriptorSet( device, aSet );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || set == VK_NULL_HANDLE ) {
        return;
    }
    if ( aDynamicOffsetCount > 0 && aDynamicOffsets == nullptr ) {
        return;
    }
    vkCmdBindDescriptorSets( impl->myCmd, ToVkBindPoint( aBindPoint ), layout, aSetIndex, 1, &set, aDynamicOffsetCount, aDynamicOffsets );
}

void CommandListPushConstants( Rhi_CommandList& aList, Rhi_PipelineLayout aLayout, Rhi_ShaderStage aStages, uint32_t aOffsetBytes, uint32_t aSizeBytes, const void* aData ) {
    CommandListImpl* impl   = AsCmd( aList );
    VkPipelineLayout layout = ResolvePipelineLayout( impl != nullptr ? impl->myDevice : nullptr, aLayout );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || aData == nullptr || aSizeBytes == 0 ) {
        return;
    }
    vkCmdPushConstants( impl->myCmd, layout, ToVkShaderStages( aStages ), aOffsetBytes, aSizeBytes, aData );
}

void CommandListDispatch( Rhi_CommandList& aList, uint32_t aGroupCountX, uint32_t aGroupCountY, uint32_t aGroupCountZ ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdDispatch( impl->myCmd, aGroupCountX, aGroupCountY, aGroupCountZ );
}

void CommandListDraw( Rhi_CommandList& aList, uint32_t aVertexCount, uint32_t aInstanceCount, uint32_t aFirstVertex, uint32_t aFirstInstance ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdDraw( impl->myCmd, aVertexCount, aInstanceCount, aFirstVertex, aFirstInstance );
}

void CommandListDrawIndexed( Rhi_CommandList& aList, uint32_t aIndexCount, uint32_t aInstanceCount, uint32_t aFirstIndex, int32_t aVertexOffset, uint32_t aFirstInstance ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdDrawIndexed( impl->myCmd, aIndexCount, aInstanceCount, aFirstIndex, aVertexOffset, aFirstInstance );
}

VkBuffer ResolveVkBuffer( CommandListImpl* aImpl, Rhi_Buffer aBuffer ) {
    if ( aImpl == nullptr || aImpl->myDevice == nullptr || aBuffer.myId == 0 ) {
        return VK_NULL_HANDLE;
    }
    auto it = aImpl->myDevice->myBuffers.find( aBuffer.myId );
    if ( it != aImpl->myDevice->myBuffers.end() ) {
        return it->second.myAlloc.myBuffer;
    }
    return reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( aBuffer.myId ) );
}

void CommandListBindVertexBuffer( Rhi_CommandList& aList, uint32_t aBinding, Rhi_Buffer aBuffer, uint64_t aOffsetBytes ) {
    CommandListImpl* impl   = AsCmd( aList );
    const VkBuffer   buffer = ResolveVkBuffer( impl, aBuffer );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE ) {
        return;
    }
    const VkDeviceSize offset = static_cast< VkDeviceSize >( aOffsetBytes );
    vkCmdBindVertexBuffers( impl->myCmd, aBinding, 1, &buffer, &offset );
}

void CommandListBindIndexBuffer( Rhi_CommandList& aList, Rhi_Buffer aBuffer, uint64_t aOffsetBytes, Rhi_IndexType aIndexType ) {
    CommandListImpl* impl   = AsCmd( aList );
    const VkBuffer   buffer = ResolveVkBuffer( impl, aBuffer );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE ) {
        return;
    }
    const VkIndexType indexType = aIndexType == Rhi_IndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer( impl->myCmd, buffer, static_cast< VkDeviceSize >( aOffsetBytes ), indexType );
}

void CommandListSetViewport( Rhi_CommandList& aList, const Viewport& aViewport ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    VkViewport vp{};
    vp.x        = aViewport.myX;
    vp.y        = aViewport.myY;
    vp.width    = aViewport.myWidth;
    vp.height   = aViewport.myHeight;
    vp.minDepth = aViewport.myMinDepth;
    vp.maxDepth = aViewport.myMaxDepth;
    vkCmdSetViewport( impl->myCmd, 0, 1, &vp );
}

void CommandListSetScissor( Rhi_CommandList& aList, const Scissor& aScissor ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    VkRect2D scissor{};
    scissor.offset = { aScissor.myX, aScissor.myY };
    scissor.extent = { aScissor.myWidth, aScissor.myHeight };
    vkCmdSetScissor( impl->myCmd, 0, 1, &scissor );
}

void CommandListSetDepthBias( Rhi_CommandList& aList, float aConstantFactor, float aClamp, float aSlopeFactor ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdSetDepthBias( impl->myCmd, aConstantFactor, aClamp, aSlopeFactor );
}

void CommandListBeginRenderPass( Rhi_CommandList& aList, const RenderPassBeginInfo& aInfo ) {
    CommandListImpl*    impl        = AsCmd( aList );
    const VkRenderPass  renderPass  = reinterpret_cast< VkRenderPass >( static_cast< uintptr_t >( aInfo.myRenderPass.myId ) );
    const VkFramebuffer framebuffer = reinterpret_cast< VkFramebuffer >( static_cast< uintptr_t >( aInfo.myFramebuffer.myId ) );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE || aInfo.myWidth == 0 || aInfo.myHeight == 0 ) {
        return;
    }
    if ( aInfo.myClearCount > 0 && aInfo.myClears == nullptr ) {
        return;
    }

    std::vector< VkClearValue > clears;
    clears.reserve( aInfo.myClearCount );
    for ( uint32_t i = 0; i < aInfo.myClearCount; ++i ) {
        VkClearValue      cv{};
        const ClearValue& src = aInfo.myClears[ i ];
        if ( src.myType == Rhi_ClearValueType::DepthStencil ) {
            cv.depthStencil.depth   = src.myDepth;
            cv.depthStencil.stencil = src.myStencil;
        }
        else {
            cv.color.float32[ 0 ] = src.myColor[ 0 ];
            cv.color.float32[ 1 ] = src.myColor[ 1 ];
            cv.color.float32[ 2 ] = src.myColor[ 2 ];
            cv.color.float32[ 3 ] = src.myColor[ 3 ];
        }
        clears.push_back( cv );
    }

    VkRenderPassBeginInfo begin{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin.renderPass        = renderPass;
    begin.framebuffer       = framebuffer;
    begin.renderArea.offset = { aInfo.myOffsetX, aInfo.myOffsetY };
    begin.renderArea.extent = { aInfo.myWidth, aInfo.myHeight };
    begin.clearValueCount   = static_cast< uint32_t >( clears.size() );
    begin.pClearValues      = clears.empty() ? nullptr : clears.data();
    vkCmdBeginRenderPass( impl->myCmd, &begin, VK_SUBPASS_CONTENTS_INLINE );
}

void CommandListEndRenderPass( Rhi_CommandList& aList ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdEndRenderPass( impl->myCmd );
}

void CommandListCopyImage( Rhi_CommandList& aList, const ImageCopy& aCopy ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || !HasLogicalDevice( impl->myDevice ) ) {
        return;
    }
    auto srcIt = impl->myDevice->myTextures.find( aCopy.mySrc.myId );
    auto dstIt = impl->myDevice->myTextures.find( aCopy.myDst.myId );
    if ( srcIt == impl->myDevice->myTextures.end() || dstIt == impl->myDevice->myTextures.end() ) {
        return;
    }
    VkImageCopy region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.extent                    = { aCopy.myWidth, aCopy.myHeight, 1 };
    vkCmdCopyImage( impl->myCmd, srcIt->second.myAlloc.myImage, ToVkLayout( aCopy.mySrcLayout ), dstIt->second.myAlloc.myImage, ToVkLayout( aCopy.myDstLayout ), 1, &region );
}

Rhi_Buffer DeviceCreateBuffer( Rhi_Device& aDevice, const BufferDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aDesc.mySizeBytes == 0 || impl->myVk->myDeviceCtx.myAllocator == nullptr ) {
        return {};
    }
    BufferRecord record{};
    impl->myVk->CreateBuffer( static_cast< VkDeviceSize >( aDesc.mySizeBytes ), ToVkBufferUsage( aDesc.myUsage ), ToVmaMemory( aDesc.myMemory ), record.myAlloc, true );
    if ( record.myAlloc.myBuffer == VK_NULL_HANDLE ) {
        return {};
    }
    const uint64_t id     = impl->myNextResourceId++;
    impl->myBuffers[ id ] = record;
    Rhi_Buffer buffer;
    buffer.myId = id;
    return buffer;
}

void DeviceDestroyBuffer( Rhi_Device& aDevice, Rhi_Buffer& aBuffer ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aBuffer.myId == 0 ) {
        return;
    }
    auto it = impl->myBuffers.find( aBuffer.myId );
    if ( it == impl->myBuffers.end() ) {
        aBuffer.myId = 0;
        return;
    }
    if ( it->second.myOwned && HasLogicalDevice( impl ) && it->second.myAlloc.myBuffer != VK_NULL_HANDLE ) {
        vmaDestroyBuffer( impl->myVk->myDeviceCtx.myAllocator, it->second.myAlloc.myBuffer, it->second.myAlloc.myAllocation );
    }
    impl->myBuffers.erase( it );
    aBuffer.myId = 0;
}

Rhi_Texture DeviceCreateTexture( Rhi_Device& aDevice, const TextureDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aDesc.myWidth == 0 || aDesc.myHeight == 0 || impl->myVk->myDeviceCtx.myAllocator == nullptr ) {
        return {};
    }
    const VkFormat format = ToVkFormat( aDesc.myFormat );
    if ( format == VK_FORMAT_UNDEFINED ) {
        return {};
    }

    TextureRecord record{};
    record.myFormat    = format;
    record.myMipLevels = aDesc.myMipLevels == 0 ? 1u : aDesc.myMipLevels;
    VkExtent2D extent{ aDesc.myWidth, aDesc.myHeight };
    impl->myVk->CreateImage( extent, format, VK_IMAGE_TILING_OPTIMAL, ToVkImageUsage( aDesc.myUsage ), ToVmaMemory( aDesc.myMemory ), record.myMipLevels,
                             VK_SAMPLE_COUNT_1_BIT, record.myAlloc );
    if ( record.myAlloc.myImage == VK_NULL_HANDLE ) {
        return {};
    }

    const VkImageAspectFlags aspect = ( format == VK_FORMAT_D32_SFLOAT ) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    record.myView                   = impl->myVk->CreateImageView( record.myAlloc.myImage, format, aspect, record.myMipLevels );
    if ( record.myView == VK_NULL_HANDLE ) {
        vmaDestroyImage( impl->myVk->myDeviceCtx.myAllocator, record.myAlloc.myImage, record.myAlloc.myAllocation );
        return {};
    }

    const uint64_t id      = impl->myNextResourceId++;
    impl->myTextures[ id ] = record;
    Rhi_Texture texture;
    texture.myId = id;
    return texture;
}

void DeviceDestroyTexture( Rhi_Device& aDevice, Rhi_Texture& aTexture ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aTexture.myId == 0 ) {
        return;
    }
    auto it = impl->myTextures.find( aTexture.myId );
    if ( it == impl->myTextures.end() ) {
        aTexture.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) ) {
        if ( it->second.myOwnsView && it->second.myView != VK_NULL_HANDLE ) {
            vkDestroyImageView( impl->myVk->myDeviceCtx.myDevice, it->second.myView, nullptr );
        }
        if ( it->second.myOwned && it->second.myAlloc.myImage != VK_NULL_HANDLE ) {
            vmaDestroyImage( impl->myVk->myDeviceCtx.myAllocator, it->second.myAlloc.myImage, it->second.myAlloc.myAllocation );
        }
    }
    impl->myTextures.erase( it );
    aTexture.myId = 0;
}

Rhi_ShaderModule DeviceCreateShaderModule( Rhi_Device& aDevice, const void* aSpirvCode, size_t aSpirvBytes ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aSpirvCode == nullptr || aSpirvBytes == 0 || ( aSpirvBytes % 4 ) != 0 ) {
        return {};
    }
    std::vector< char > bytes( static_cast< const char* >( aSpirvCode ), static_cast< const char* >( aSpirvCode ) + aSpirvBytes );
    VkShaderModule      module = impl->myVk->CreateShaderModule( bytes );
    if ( module == VK_NULL_HANDLE ) {
        return {};
    }
    const uint64_t id     = impl->myNextResourceId++;
    impl->myShaders[ id ] = module;
    Rhi_ShaderModule out;
    out.myId = id;
    return out;
}

void DeviceDestroyShaderModule( Rhi_Device& aDevice, Rhi_ShaderModule& aModule ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aModule.myId == 0 ) {
        return;
    }
    auto it = impl->myShaders.find( aModule.myId );
    if ( it == impl->myShaders.end() ) {
        aModule.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroyShaderModule( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->myShaders.erase( it );
    aModule.myId = 0;
}

VkFilter ToVkFilter( Rhi_Filter aFilter ) {
    return aFilter == Rhi_Filter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

VkSamplerMipmapMode ToVkMipmapMode( Rhi_MipmapMode aMode ) {
    return aMode == Rhi_MipmapMode::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode ToVkAddressMode( Rhi_AddressMode aMode ) {
    switch ( aMode ) {
    case Rhi_AddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case Rhi_AddressMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case Rhi_AddressMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case Rhi_AddressMode::ClampToEdge:
    default:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
}

VkDescriptorType ToVkDescriptorType( Rhi_DescriptorType aType ) {
    switch ( aType ) {
    case Rhi_DescriptorType::Sampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case Rhi_DescriptorType::SampledImage:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case Rhi_DescriptorType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case Rhi_DescriptorType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case Rhi_DescriptorType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case Rhi_DescriptorType::CombinedImageSampler:
    default:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
}

Rhi_Sampler DeviceCreateSampler( Rhi_Device& aDevice, const SamplerDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) ) {
        return {};
    }
    VkSamplerCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter     = ToVkFilter( aDesc.myMagFilter );
    info.minFilter     = ToVkFilter( aDesc.myMinFilter );
    info.mipmapMode    = ToVkMipmapMode( aDesc.myMipmapMode );
    info.addressModeU  = ToVkAddressMode( aDesc.myAddressU );
    info.addressModeV  = ToVkAddressMode( aDesc.myAddressV );
    info.addressModeW  = ToVkAddressMode( aDesc.myAddressW );
    info.maxLod        = aDesc.myMaxLod;
    info.compareEnable = aDesc.myCompareEnable ? VK_TRUE : VK_FALSE;
    VkSampler sampler  = VK_NULL_HANDLE;
    if ( vkCreateSampler( impl->myVk->myDeviceCtx.myDevice, &info, nullptr, &sampler ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id      = impl->myNextResourceId++;
    impl->mySamplers[ id ] = sampler;
    Rhi_Sampler out;
    out.myId = id;
    return out;
}

void DeviceDestroySampler( Rhi_Device& aDevice, Rhi_Sampler& aSampler ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aSampler.myId == 0 ) {
        return;
    }
    auto it = impl->mySamplers.find( aSampler.myId );
    if ( it == impl->mySamplers.end() ) {
        aSampler.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroySampler( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->mySamplers.erase( it );
    aSampler.myId = 0;
}

Rhi_DescriptorSetLayout DeviceCreateDescriptorSetLayout( Rhi_Device& aDevice, const DescriptorSetLayoutDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aDesc.myBindings == nullptr || aDesc.myBindingCount == 0 ) {
        return {};
    }
    std::vector< VkDescriptorSetLayoutBinding > bindings( aDesc.myBindingCount );
    for ( uint32_t i = 0; i < aDesc.myBindingCount; ++i ) {
        bindings[ i ].binding         = aDesc.myBindings[ i ].myBinding;
        bindings[ i ].descriptorType  = ToVkDescriptorType( aDesc.myBindings[ i ].myType );
        bindings[ i ].descriptorCount = aDesc.myBindings[ i ].myCount == 0 ? 1u : aDesc.myBindings[ i ].myCount;
        bindings[ i ].stageFlags      = ToVkShaderStages( aDesc.myBindings[ i ].myStages );
    }
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount            = aDesc.myBindingCount;
    info.pBindings               = bindings.data();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if ( vkCreateDescriptorSetLayout( impl->myVk->myDeviceCtx.myDevice, &info, nullptr, &layout ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id        = impl->myNextResourceId++;
    impl->mySetLayouts[ id ] = layout;
    Rhi_DescriptorSetLayout out;
    out.myId = id;
    return out;
}

void DeviceDestroyDescriptorSetLayout( Rhi_Device& aDevice, Rhi_DescriptorSetLayout& aLayout ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aLayout.myId == 0 ) {
        return;
    }
    auto it = impl->mySetLayouts.find( aLayout.myId );
    if ( it == impl->mySetLayouts.end() ) {
        aLayout.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroyDescriptorSetLayout( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->mySetLayouts.erase( it );
    aLayout.myId = 0;
}

Rhi_DescriptorPool DeviceCreateDescriptorPool( Rhi_Device& aDevice, const DescriptorPoolDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aDesc.myMaxSets == 0 || aDesc.myPoolSizes == nullptr || aDesc.myPoolSizeCount == 0 ) {
        return {};
    }
    std::vector< VkDescriptorPoolSize > sizes( aDesc.myPoolSizeCount );
    for ( uint32_t i = 0; i < aDesc.myPoolSizeCount; ++i ) {
        sizes[ i ].type            = ToVkDescriptorType( aDesc.myPoolSizes[ i ].myType );
        sizes[ i ].descriptorCount = aDesc.myPoolSizes[ i ].myCount;
    }
    VkDescriptorPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets          = aDesc.myMaxSets;
    info.poolSizeCount    = aDesc.myPoolSizeCount;
    info.pPoolSizes       = sizes.data();
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if ( vkCreateDescriptorPool( impl->myVk->myDeviceCtx.myDevice, &info, nullptr, &pool ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id   = impl->myNextResourceId++;
    impl->myPools[ id ] = pool;
    Rhi_DescriptorPool out;
    out.myId = id;
    return out;
}

void DeviceDestroyDescriptorPool( Rhi_Device& aDevice, Rhi_DescriptorPool& aPool ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aPool.myId == 0 ) {
        return;
    }
    auto it = impl->myPools.find( aPool.myId );
    if ( it == impl->myPools.end() ) {
        aPool.myId = 0;
        return;
    }
    // Drop sets allocated from this pool only.
    for ( auto setIt = impl->myDescriptorSets.begin(); setIt != impl->myDescriptorSets.end(); ) {
        if ( setIt->second.second == aPool.myId ) {
            setIt = impl->myDescriptorSets.erase( setIt );
        }
        else {
            ++setIt;
        }
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroyDescriptorPool( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->myPools.erase( it );
    aPool.myId = 0;
}

Rhi_DescriptorSet DeviceAllocateDescriptorSet( Rhi_Device& aDevice, Rhi_DescriptorPool aPool, Rhi_DescriptorSetLayout aLayout ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || !aPool || !aLayout ) {
        return {};
    }
    auto poolIt = impl->myPools.find( aPool.myId );
    auto layIt  = impl->mySetLayouts.find( aLayout.myId );
    if ( poolIt == impl->myPools.end() || layIt == impl->mySetLayouts.end() ) {
        return {};
    }
    VkDescriptorSetAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = poolIt->second;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layIt->second;
    VkDescriptorSet set     = VK_NULL_HANDLE;
    if ( vkAllocateDescriptorSets( impl->myVk->myDeviceCtx.myDevice, &info, &set ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id            = impl->myNextResourceId++;
    impl->myDescriptorSets[ id ] = { set, aPool.myId };
    Rhi_DescriptorSet out;
    out.myId = id;
    return out;
}

Rhi_PipelineLayout DeviceCreatePipelineLayout( Rhi_Device& aDevice, const PipelineLayoutDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) ) {
        return {};
    }
    std::vector< VkDescriptorSetLayout > setLayouts;
    setLayouts.reserve( aDesc.mySetLayoutCount );
    for ( uint32_t i = 0; i < aDesc.mySetLayoutCount; ++i ) {
        auto it = impl->mySetLayouts.find( aDesc.mySetLayouts[ i ].myId );
        if ( it == impl->mySetLayouts.end() ) {
            return {};
        }
        setLayouts.push_back( it->second );
    }
    std::vector< VkPushConstantRange > pushes;
    pushes.reserve( aDesc.myPushRangeCount );
    for ( uint32_t i = 0; i < aDesc.myPushRangeCount; ++i ) {
        VkPushConstantRange range{};
        range.stageFlags = ToVkShaderStages( aDesc.myPushRanges[ i ].myStages );
        range.offset     = aDesc.myPushRanges[ i ].myOffsetBytes;
        range.size       = aDesc.myPushRanges[ i ].mySizeBytes;
        pushes.push_back( range );
    }
    VkPipelineLayoutCreateInfo info{};
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount         = static_cast< uint32_t >( setLayouts.size() );
    info.pSetLayouts            = setLayouts.empty() ? nullptr : setLayouts.data();
    info.pushConstantRangeCount = static_cast< uint32_t >( pushes.size() );
    info.pPushConstantRanges    = pushes.empty() ? nullptr : pushes.data();
    VkPipelineLayout layout     = VK_NULL_HANDLE;
    if ( vkCreatePipelineLayout( impl->myVk->myDeviceCtx.myDevice, &info, nullptr, &layout ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id             = impl->myNextResourceId++;
    impl->myPipelineLayouts[ id ] = layout;
    Rhi_PipelineLayout out;
    out.myId = id;
    return out;
}

void DeviceDestroyPipelineLayout( Rhi_Device& aDevice, Rhi_PipelineLayout& aLayout ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aLayout.myId == 0 ) {
        return;
    }
    auto it = impl->myPipelineLayouts.find( aLayout.myId );
    if ( it == impl->myPipelineLayouts.end() ) {
        aLayout.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroyPipelineLayout( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->myPipelineLayouts.erase( it );
    aLayout.myId = 0;
}

Rhi_Pipeline DeviceCreateComputePipeline( Rhi_Device& aDevice, const ComputePipelineDesc& aDesc ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || !aDesc.myShader || !aDesc.myLayout ) {
        return {};
    }
    auto shaderIt = impl->myShaders.find( aDesc.myShader.myId );
    auto layoutIt = impl->myPipelineLayouts.find( aDesc.myLayout.myId );
    if ( shaderIt == impl->myShaders.end() || layoutIt == impl->myPipelineLayouts.end() ) {
        return {};
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shaderIt->second;
    stage.pName  = aDesc.myEntry != nullptr ? aDesc.myEntry : "main";

    VkComputePipelineCreateInfo info{};
    info.sType          = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage          = stage;
    info.layout         = layoutIt->second;
    VkPipeline pipeline = VK_NULL_HANDLE;
    if ( vkCreateComputePipelines( impl->myVk->myDeviceCtx.myDevice, impl->myVk->myDeviceCtx.myPipelineCache, 1, &info, nullptr, &pipeline ) != VK_SUCCESS ) {
        return {};
    }
    const uint64_t id       = impl->myNextResourceId++;
    impl->myPipelines[ id ] = pipeline;
    Rhi_Pipeline out;
    out.myId = id;
    return out;
}

void DeviceDestroyPipeline( Rhi_Device& aDevice, Rhi_Pipeline& aPipeline ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aPipeline.myId == 0 ) {
        return;
    }
    auto it = impl->myPipelines.find( aPipeline.myId );
    if ( it == impl->myPipelines.end() ) {
        aPipeline.myId = 0;
        return;
    }
    if ( HasLogicalDevice( impl ) && it->second != VK_NULL_HANDLE ) {
        vkDestroyPipeline( impl->myVk->myDeviceCtx.myDevice, it->second, nullptr );
    }
    impl->myPipelines.erase( it );
    aPipeline.myId = 0;
}

Rhi_Texture DeviceCreateTextureMipView( Rhi_Device& aDevice, Rhi_Texture aParent, uint32_t aBaseMip ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || !aParent ) {
        return {};
    }
    auto parentIt = impl->myTextures.find( aParent.myId );
    if ( parentIt == impl->myTextures.end() || parentIt->second.myAlloc.myImage == VK_NULL_HANDLE ) {
        return {};
    }
    if ( aBaseMip >= parentIt->second.myMipLevels ) {
        return {};
    }
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = parentIt->second.myAlloc.myImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = parentIt->second.myFormat;
    viewInfo.subresourceRange.aspectMask     = ( parentIt->second.myFormat == VK_FORMAT_D32_SFLOAT ) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = aBaseMip;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    VkImageView view                         = VK_NULL_HANDLE;
    if ( vkCreateImageView( impl->myVk->myDeviceCtx.myDevice, &viewInfo, nullptr, &view ) != VK_SUCCESS ) {
        return {};
    }
    TextureRecord record{};
    record.myAlloc              = parentIt->second.myAlloc;
    record.myAlloc.myAllocation = VK_NULL_HANDLE;  // not owning allocation
    record.myView               = view;
    record.myFormat             = parentIt->second.myFormat;
    record.myMipLevels          = 1;
    record.myOwned              = false;
    record.myOwnsView           = true;
    const uint64_t id           = impl->myNextResourceId++;
    impl->myTextures[ id ]      = record;
    Rhi_Texture out;
    out.myId = id;
    return out;
}

void DeviceUpdateDescriptorImages( Rhi_Device& aDevice, const DescriptorImageWrite* aWrites, uint32_t aWriteCount ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( !HasLogicalDevice( impl ) || aWrites == nullptr || aWriteCount == 0 ) {
        return;
    }
    std::vector< VkDescriptorImageInfo > imageInfos( aWriteCount );
    std::vector< VkWriteDescriptorSet >  writes( aWriteCount );
    for ( uint32_t i = 0; i < aWriteCount; ++i ) {
        const DescriptorImageWrite& w     = aWrites[ i ];
        VkDescriptorSet             set   = VK_NULL_HANDLE;
        auto                        setIt = impl->myDescriptorSets.find( w.mySet.myId );
        if ( setIt != impl->myDescriptorSets.end() ) {
            set = setIt->second.first;
        }
        else {
            set = reinterpret_cast< VkDescriptorSet >( static_cast< uintptr_t >( w.mySet.myId ) );
        }
        VkSampler sampler = VK_NULL_HANDLE;
        if ( w.mySampler ) {
            auto sampIt = impl->mySamplers.find( w.mySampler.myId );
            if ( sampIt != impl->mySamplers.end() ) {
                sampler = sampIt->second;
            }
        }
        VkImageView view  = VK_NULL_HANDLE;
        auto        texIt = impl->myTextures.find( w.myTexture.myId );
        if ( texIt != impl->myTextures.end() ) {
            view = texIt->second.myView;
        }
        imageInfos[ i ].sampler     = sampler;
        imageInfos[ i ].imageView   = view;
        imageInfos[ i ].imageLayout = ToVkLayout( w.myLayout );

        writes[ i ]                 = {};
        writes[ i ].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[ i ].dstSet          = set;
        writes[ i ].dstBinding      = w.myBinding;
        writes[ i ].descriptorCount = 1;
        writes[ i ].descriptorType  = ToVkDescriptorType( w.myType );
        writes[ i ].pImageInfo      = &imageInfos[ i ];
    }
    vkUpdateDescriptorSets( impl->myVk->myDeviceCtx.myDevice, aWriteCount, writes.data(), 0, nullptr );
}

}  // namespace Rhi

namespace RhiVulkan {

Rhi_Device DeviceWrap( Vk_RhiDevice& aDevice ) {
    auto* impl       = new DeviceImpl();
    impl->myVk       = &aDevice;
    impl->myOwnsVk   = false;
    impl->myRefCount = 1;
    return MakeDeviceHandle( impl );
}

Vk_RhiDevice* DeviceGetVk( const Rhi_Device& aDevice ) {
    DeviceImpl* impl = AsDevice( aDevice );
    return impl != nullptr ? impl->myVk : nullptr;
}

void DeviceSetDebugUtils( const Rhi_Device& aDevice, PFN_vkCmdBeginDebugUtilsLabelEXT aBegin, PFN_vkCmdEndDebugUtilsLabelEXT aEnd ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return;
    }
    impl->myCmdBeginLabel = aBegin;
    impl->myCmdEndLabel   = aEnd;
}

void CommandListBindVk( Rhi_CommandList& aList, VkCommandBuffer aCommandBuffer ) {
    CommandListImpl* impl = AsCmd( aList );
    if ( impl == nullptr ) {
        return;
    }
    impl->myCmd = aCommandBuffer;
}

VkCommandBuffer CommandListGetVk( const Rhi_CommandList& aList ) {
    const CommandListImpl* impl = AsCmd( aList );
    return impl != nullptr ? impl->myCmd : VK_NULL_HANDLE;
}

Rhi_Pipeline PipelineAdopt( VkPipeline aPipeline ) {
    Rhi_Pipeline out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aPipeline ) );
    return out;
}

Rhi_PipelineLayout PipelineLayoutAdopt( VkPipelineLayout aLayout ) {
    Rhi_PipelineLayout out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aLayout ) );
    return out;
}

Rhi_DescriptorSet DescriptorSetAdopt( VkDescriptorSet aSet ) {
    Rhi_DescriptorSet out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aSet ) );
    return out;
}

Rhi_RenderPass RenderPassAdopt( VkRenderPass aRenderPass ) {
    Rhi_RenderPass out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aRenderPass ) );
    return out;
}

Rhi_Framebuffer FramebufferAdopt( VkFramebuffer aFramebuffer ) {
    Rhi_Framebuffer out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aFramebuffer ) );
    return out;
}

Rhi_Texture TextureAdopt( const Rhi_Device& aDevice, VkImage aImage, VkImageView aView, VkFormat aFormat, uint32_t aMipLevels ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr || aImage == VK_NULL_HANDLE ) {
        return {};
    }
    TextureRecord record{};
    record.myAlloc.myImage      = aImage;
    record.myAlloc.myAllocation = VK_NULL_HANDLE;
    record.myView               = aView;
    record.myFormat             = aFormat;
    record.myMipLevels          = aMipLevels == 0 ? 1u : aMipLevels;
    record.myOwned              = false;
    record.myOwnsView           = false;
    const uint64_t id           = impl->myNextResourceId++;
    impl->myTextures[ id ]      = record;
    Rhi_Texture texture;
    texture.myId = id;
    return texture;
}

Rhi_Buffer BufferAdopt( VkBuffer aBuffer ) {
    Rhi_Buffer out;
    out.myId = static_cast< uint64_t >( reinterpret_cast< uintptr_t >( aBuffer ) );
    return out;
}

VkPipeline PipelineGetVk( const Rhi_Device& aDevice, Rhi_Pipeline aPipeline ) {
    return ResolvePipeline( AsDevice( aDevice ), aPipeline );
}

VkPipelineLayout PipelineLayoutGetVk( const Rhi_Device& aDevice, Rhi_PipelineLayout aLayout ) {
    return ResolvePipelineLayout( AsDevice( aDevice ), aLayout );
}

VkDescriptorSet DescriptorSetGetVk( const Rhi_Device& aDevice, Rhi_DescriptorSet aSet ) {
    return ResolveDescriptorSet( AsDevice( aDevice ), aSet );
}

VkPipeline PipelineGetVk( Rhi_Pipeline aPipeline ) {
    return ResolvePipeline( nullptr, aPipeline );
}

VkPipelineLayout PipelineLayoutGetVk( Rhi_PipelineLayout aLayout ) {
    return ResolvePipelineLayout( nullptr, aLayout );
}

VkDescriptorSet DescriptorSetGetVk( Rhi_DescriptorSet aSet ) {
    return ResolveDescriptorSet( nullptr, aSet );
}

VkRenderPass RenderPassGetVk( Rhi_RenderPass aRenderPass ) {
    return reinterpret_cast< VkRenderPass >( static_cast< uintptr_t >( aRenderPass.myId ) );
}

VkFramebuffer FramebufferGetVk( Rhi_Framebuffer aFramebuffer ) {
    return reinterpret_cast< VkFramebuffer >( static_cast< uintptr_t >( aFramebuffer.myId ) );
}

VkBuffer BufferGetVk( const Rhi_Device& aDevice, Rhi_Buffer aBuffer ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->myBuffers.find( aBuffer.myId );
    if ( it != impl->myBuffers.end() ) {
        return it->second.myAlloc.myBuffer;
    }
    // Non-owned adopt path: id is the VkBuffer pointer bits.
    return reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( aBuffer.myId ) );
}

VkImage TextureGetVkImage( const Rhi_Device& aDevice, Rhi_Texture aTexture ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->myTextures.find( aTexture.myId );
    if ( it == impl->myTextures.end() ) {
        return VK_NULL_HANDLE;
    }
    return it->second.myAlloc.myImage;
}

VkImageView TextureGetVkView( const Rhi_Device& aDevice, Rhi_Texture aTexture ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->myTextures.find( aTexture.myId );
    if ( it == impl->myTextures.end() ) {
        return VK_NULL_HANDLE;
    }
    return it->second.myView;
}

VkSampler SamplerGetVk( const Rhi_Device& aDevice, Rhi_Sampler aSampler ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->mySamplers.find( aSampler.myId );
    if ( it == impl->mySamplers.end() ) {
        return VK_NULL_HANDLE;
    }
    return it->second;
}

VkDescriptorSetLayout DescriptorSetLayoutGetVk( const Rhi_Device& aDevice, Rhi_DescriptorSetLayout aLayout ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->mySetLayouts.find( aLayout.myId );
    if ( it == impl->mySetLayouts.end() ) {
        return VK_NULL_HANDLE;
    }
    return it->second;
}

VkDescriptorPool DescriptorPoolGetVk( const Rhi_Device& aDevice, Rhi_DescriptorPool aPool ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->myPools.find( aPool.myId );
    if ( it == impl->myPools.end() ) {
        return VK_NULL_HANDLE;
    }
    return it->second;
}

}  // namespace RhiVulkan

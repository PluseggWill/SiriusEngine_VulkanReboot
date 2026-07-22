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
    bool              myOwned     = true;  // false when TextureAdopt
};

struct DeviceImpl;

struct CommandListImpl {
    DeviceImpl*     myDevice = nullptr;
    VkCommandBuffer myCmd    = VK_NULL_HANDLE;
};

struct DeviceImpl {
    Vk_RhiDevice*                                  myVk       = nullptr;
    bool                                           myOwnsVk   = false;
    int                                            myRefCount = 1;
    std::vector< CommandListImpl* >                myCommandLists;
    PFN_vkCmdBeginDebugUtilsLabelEXT               myCmdBeginLabel  = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT                 myCmdEndLabel    = nullptr;
    uint64_t                                       myNextResourceId = 1;
    std::unordered_map< uint64_t, BufferRecord >   myBuffers;
    std::unordered_map< uint64_t, TextureRecord >  myTextures;
    std::unordered_map< uint64_t, VkShaderModule > myShaders;
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
        return;
    }

    for ( auto& [ id, shader ] : aImpl->myShaders ) {
        ( void )id;
        if ( shader != VK_NULL_HANDLE ) {
            vkDestroyShaderModule( device, shader, nullptr );
        }
    }
    aImpl->myShaders.clear();

    for ( auto& [ id, tex ] : aImpl->myTextures ) {
        ( void )id;
        if ( tex.myView != VK_NULL_HANDLE && tex.myOwned ) {
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
        const BufferBarrier& b        = aBarriers[ i ];
        VkBuffer             resolved = VK_NULL_HANDLE;
        auto                 it       = impl->myDevice->myBuffers.find( b.myBuffer.myId );
        if ( it != impl->myDevice->myBuffers.end() ) {
            resolved = it->second.myAlloc.myBuffer;
        }
        else {
            resolved = reinterpret_cast< VkBuffer >( static_cast< uintptr_t >( b.myBuffer.myId ) );
        }
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

void CommandListBindPipeline( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_Pipeline aPipeline ) {
    CommandListImpl* impl = AsCmd( aList );
    VkPipeline       pipe = reinterpret_cast< VkPipeline >( static_cast< uintptr_t >( aPipeline.myId ) );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || pipe == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdBindPipeline( impl->myCmd, ToVkBindPoint( aBindPoint ), pipe );
}

void CommandListBindDescriptorSet( Rhi_CommandList& aList, Rhi_PipelineBindPoint aBindPoint, Rhi_PipelineLayout aLayout, uint32_t aSetIndex, Rhi_DescriptorSet aSet ) {
    CommandListImpl* impl   = AsCmd( aList );
    VkPipelineLayout layout = reinterpret_cast< VkPipelineLayout >( static_cast< uintptr_t >( aLayout.myId ) );
    VkDescriptorSet  set    = reinterpret_cast< VkDescriptorSet >( static_cast< uintptr_t >( aSet.myId ) );
    if ( impl == nullptr || impl->myCmd == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || set == VK_NULL_HANDLE ) {
        return;
    }
    vkCmdBindDescriptorSets( impl->myCmd, ToVkBindPoint( aBindPoint ), layout, aSetIndex, 1, &set, 0, nullptr );
}

void CommandListPushConstants( Rhi_CommandList& aList, Rhi_PipelineLayout aLayout, Rhi_ShaderStage aStages, uint32_t aOffsetBytes, uint32_t aSizeBytes, const void* aData ) {
    CommandListImpl* impl   = AsCmd( aList );
    VkPipelineLayout layout = reinterpret_cast< VkPipelineLayout >( static_cast< uintptr_t >( aLayout.myId ) );
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
    if ( it->second.myOwned && HasLogicalDevice( impl ) ) {
        if ( it->second.myView != VK_NULL_HANDLE ) {
            vkDestroyImageView( impl->myVk->myDeviceCtx.myDevice, it->second.myView, nullptr );
        }
        if ( it->second.myAlloc.myImage != VK_NULL_HANDLE ) {
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

VkPipeline PipelineGetVk( Rhi_Pipeline aPipeline ) {
    return reinterpret_cast< VkPipeline >( static_cast< uintptr_t >( aPipeline.myId ) );
}

VkPipelineLayout PipelineLayoutGetVk( Rhi_PipelineLayout aLayout ) {
    return reinterpret_cast< VkPipelineLayout >( static_cast< uintptr_t >( aLayout.myId ) );
}

VkDescriptorSet DescriptorSetGetVk( Rhi_DescriptorSet aSet ) {
    return reinterpret_cast< VkDescriptorSet >( static_cast< uintptr_t >( aSet.myId ) );
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
    return it != impl->myTextures.end() ? it->second.myAlloc.myImage : VK_NULL_HANDLE;
}

VkImageView TextureGetVkView( const Rhi_Device& aDevice, Rhi_Texture aTexture ) {
    DeviceImpl* impl = AsDevice( aDevice );
    if ( impl == nullptr ) {
        return VK_NULL_HANDLE;
    }
    auto it = impl->myTextures.find( aTexture.myId );
    return it != impl->myTextures.end() ? it->second.myView : VK_NULL_HANDLE;
}

}  // namespace RhiVulkan

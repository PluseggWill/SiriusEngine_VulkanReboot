// Module: Vk_GpuCull — compute frustum cull from entity-record SSBO to slot-indexed indirect buffer.
#include "Vk_GpuCull.h"

#include "../Gfx/Gfx_GpuCull.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"

#include <array>
#include <stdexcept>

#include <vulkan/vulkan.h>

namespace {

constexpr char     kEntityCullShaderPath[] = "VulkanDesktop/Shader_Generated/EntityCull.spv";
constexpr uint32_t kCullWorkgroupSize      = 64;

VkBufferMemoryBarrier BufferBarrier( VkBuffer aBuffer, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = aSrcAccess;
    barrier.dstAccessMask = aDstAccess;
    barrier.buffer        = aBuffer;
    barrier.offset        = 0;
    barrier.size          = VK_WHOLE_SIZE;
    return barrier;
}

void CmdPipelineBarrierBuffer( VkCommandBuffer aCommandBuffer, VkPipelineStageFlags aSrcStage, VkPipelineStageFlags aDstStage, const VkBufferMemoryBarrier& aBarrier ) {
    vkCmdPipelineBarrier( aCommandBuffer, aSrcStage, aDstStage, 0, 0, nullptr, 1, &aBarrier, 0, nullptr );
}

}  // namespace

void Vk_GpuCull::Init( Vk_Core& aCore ) {
    Vk_GpuCullState& state = aCore.myGpuCullState;

    VkShaderModule computeModule = aCore.CreateShaderModule( kEntityCullShaderPath );

    const std::array< VkDescriptorSetLayoutBinding, 3 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myDescriptorSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_GpuCull: failed to create descriptor set layout" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( Gfx_GpuCullPushConstants );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.myDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_GpuCull: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = state.myPipelineLayout;
    if ( vkCreateComputePipelines( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &state.myComputePipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_GpuCull: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, computeModule, nullptr );

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * 3;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_GpuCull: failed to create descriptor pool" );
    }

    const VkDevice              device          = aCore.myDeviceCtx.myDevice;
    const VkPipeline            computePipeline = state.myComputePipeline;
    const VkPipelineLayout      pipelineLayout  = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout       = state.myDescriptorSetLayout;
    const VkDescriptorPool      descriptorPool  = state.myDescriptorPool;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, computePipeline, pipelineLayout, setLayout, descriptorPool ]() {
        if ( computePipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( device, computePipeline, nullptr );
        }
        if ( pipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        }
        if ( setLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, setLayout, nullptr );
        }
        if ( descriptorPool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        }
    } );

    UtilLogger::Info( "PIPELINE", "GPU cull compute pipeline created." );
}

void Vk_GpuCull::CreateFrameBuffers( Vk_Core& aCore ) {
    const VkDeviceSize indirectBytes = static_cast< VkDeviceSize >( VkDescriptorPolicy::kMaxEntitySlots ) * sizeof( VkDrawIndexedIndirectCommand );

    for ( int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        Vk_FrameData& frame = aCore.myFrameCtx.myFrameDatas[ i ];

        aCore.CreateBuffer( indirectBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
                            frame.myGpuCullIndirectBuffer, true );
        aCore.CreateBuffer( sizeof( uint32_t ), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, frame.myGpuCullVisibleCountBuffer, true );

        void* indirectMapped = nullptr;
        vmaMapMemory( aCore.myDeviceCtx.myAllocator, frame.myGpuCullIndirectBuffer.myAllocation, &indirectMapped );
        frame.myGpuCullIndirectMapped = indirectMapped;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = aCore.myGpuCullState.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &aCore.myGpuCullState.myDescriptorSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &frame.myGpuCullDescriptorSet ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_GpuCull: failed to allocate descriptor set" );
        }

        VkDescriptorBufferInfo entityInfo{};
        entityInfo.buffer = frame.myEntityRecordBuffer.myBuffer;
        entityInfo.offset = 0;
        entityInfo.range  = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo indirectInfo{};
        indirectInfo.buffer = frame.myGpuCullIndirectBuffer.myBuffer;
        indirectInfo.offset = 0;
        indirectInfo.range  = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo counterInfo{};
        counterInfo.buffer = frame.myGpuCullVisibleCountBuffer.myBuffer;
        counterInfo.offset = 0;
        counterInfo.range  = sizeof( uint32_t );

        std::array< VkWriteDescriptorSet, 3 > writes = {
            VkInit::DescriptorSetWriteCreateInfo( frame.myGpuCullDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &entityInfo, 0, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( frame.myGpuCullDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectInfo, 1, 1 ),
            VkInit::DescriptorSetWriteCreateInfo( frame.myGpuCullDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &counterInfo, 2, 1 ),
        };
        vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );

        const VmaAllocator       allocator         = aCore.myDeviceCtx.myAllocator;
        const Vk_AllocatedBuffer indirectBuffer    = frame.myGpuCullIndirectBuffer;
        const Vk_AllocatedBuffer counterBuffer     = frame.myGpuCullVisibleCountBuffer;
        void* const              indirectMappedPtr = frame.myGpuCullIndirectMapped;
        aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ allocator, indirectBuffer, counterBuffer, indirectMappedPtr ]() {
            if ( indirectMappedPtr != nullptr ) {
                vmaUnmapMemory( allocator, indirectBuffer.myAllocation );
            }
            vmaDestroyBuffer( allocator, indirectBuffer.myBuffer, indirectBuffer.myAllocation );
            vmaDestroyBuffer( allocator, counterBuffer.myBuffer, counterBuffer.myAllocation );
        } );
    }

    UtilLogger::Info( "RESOURCE",
                      "GPU cull buffers: slots=" + std::to_string( VkDescriptorPolicy::kMaxEntitySlots ) + " bytes/indirect/frame=" + std::to_string( indirectBytes ) );
}

void Vk_GpuCull::RecordDispatches( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Vk_FrameCpuPrepResult& aPrep ) {
    if ( !aCore.EngineConfig().GetGpuCullEnabled() || aPrep.myFrameData == nullptr ) {
        return;
    }

    Vk_FrameData&    frame = *aPrep.myFrameData;
    Vk_GpuCullState& state = aCore.myGpuCullState;

    static bool sDispatchLoggedOnce = false;

    const VkBufferMemoryBarrier entityBarrier = BufferBarrier( frame.myEntityRecordBuffer.myBuffer, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
    CmdPipelineBarrierBuffer( aCommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, entityBarrier );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myComputePipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, state.myPipelineLayout, 0, 1, &frame.myGpuCullDescriptorSet, 0, nullptr );

    for ( uint32_t viewIndex = 0; viewIndex < aPrep.myActiveViewCount; ++viewIndex ) {
        vkCmdFillBuffer( aCommandBuffer, frame.myGpuCullVisibleCountBuffer.myBuffer, 0, sizeof( uint32_t ), 0 );

        const VkBufferMemoryBarrier counterBarrier =
            BufferBarrier( frame.myGpuCullVisibleCountBuffer.myBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );
        CmdPipelineBarrierBuffer( aCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, counterBarrier );

        const Gfx_GpuCullPushConstants& params = aPrep.myGpuCullViews[ viewIndex ];
        vkCmdPushConstants( aCommandBuffer, state.myPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( Gfx_GpuCullPushConstants ), &params );

        const uint32_t groupCount = ( params.slotCount + kCullWorkgroupSize - 1 ) / kCullWorkgroupSize;
        if ( groupCount > 0 ) {
            vkCmdDispatch( aCommandBuffer, groupCount, 1, 1 );
        }
    }

    const VkBufferMemoryBarrier indirectBarrier =
        BufferBarrier( frame.myGpuCullIndirectBuffer.myBuffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT );
    CmdPipelineBarrierBuffer( aCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, indirectBarrier );

    if ( !sDispatchLoggedOnce ) {
        UtilLogger::Info( "CULL", "GPU cull dispatch: views=" + std::to_string( aPrep.myActiveViewCount ) + " slots=" + std::to_string( aPrep.mySceneSlotCount ) );
        sDispatchLoggedOnce = true;
    }
}

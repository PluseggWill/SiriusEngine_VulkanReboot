// Module: Vk_ClusterBuildPass — FG v0 clustered light list build (compute).
#include "Vk_ClusterBuildPass.h"

#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <stdexcept>

namespace {

constexpr char kClusterBuildShaderPath[] = "VulkanDesktop/Shader_Generated/ClusterBuild.spv";

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

void DestroyClusterListBuffers( Vk_Renderer& aCore, bool aClearDescriptorSets ) {
    Vk_ClusterBuildState& state     = aCore.myClusterBuildState;
    const VmaAllocator    allocator = aCore.myRhi.myDeviceCtx.myAllocator;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        if ( state.myClusterListBuffers[ i ].myBuffer != VK_NULL_HANDLE ) {
            vmaDestroyBuffer( allocator, state.myClusterListBuffers[ i ].myBuffer, state.myClusterListBuffers[ i ].myAllocation );
            state.myClusterListBuffers[ i ] = {};
        }
        if ( aClearDescriptorSets ) {
            state.myDescriptorSets[ i ] = VK_NULL_HANDLE;
        }
    }
    state.myClusterCount = 0;
}

void WriteSunLightFromEnvironment( Gpu_ClusterLight& aOut, const Gpu_EnvironmentData& aEnv ) {
    const glm::vec3 sunDir = glm::normalize( glm::vec3( aEnv.mySunlightDirection ) );
    const glm::vec3 sunCol = glm::vec3( aEnv.mySunlightColor ) * aEnv.mySunlightColor.w;
    std::memcpy( aOut.direction, glm::value_ptr( glm::vec4( sunDir, 0.0f ) ), sizeof( float ) * 4 );
    std::memcpy( aOut.color, glm::value_ptr( glm::vec4( sunCol, 1.0f ) ), sizeof( float ) * 4 );
}

void UpdateDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_ClusterBuildState& state = aCore.myClusterBuildState;

    VkDescriptorBufferInfo lightsInfo{};
    lightsInfo.buffer = state.myLightsBuffer.myBuffer;
    lightsInfo.offset = 0;
    lightsInfo.range  = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;

    VkDescriptorBufferInfo listsInfo{};
    listsInfo.buffer = state.myClusterListBuffers[ aFrameIndex ].myBuffer;
    listsInfo.offset = 0;
    listsInfo.range  = VK_WHOLE_SIZE;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightsInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &listsInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void AllocateClusterListBuffers( Vk_Renderer& aCore, bool aAllocateDescriptors ) {
    Vk_ClusterBuildState& state = aCore.myClusterBuildState;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    state.myClusterCount  = Gfx_ClusterLighting::ClusterCount( width, height );

    const VkDeviceSize listBytes = static_cast< VkDeviceSize >( state.myClusterCount ) * sizeof( Gpu_ClusterLightList );

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        aCore.CreateBuffer( listBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, state.myClusterListBuffers[ i ], false );

        if ( aAllocateDescriptors ) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = state.myDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts        = &state.myDescriptorSetLayout;
            if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "Vk_ClusterBuildPass: failed to allocate descriptor set" );
            }
        }
        UpdateDescriptorSet( aCore, i );
    }

    UtilLogger::Info( "CLUSTER", "Cluster lists: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " clusters=" + std::to_string( state.myClusterCount )
                                     + " bytes/list/frame=" + std::to_string( listBytes ) );
}

void CreatePipeline( Vk_Renderer& aCore ) {
    Vk_ClusterBuildState& state         = aCore.myClusterBuildState;
    const std::string     spvPath       = UtilLoader::ResolvePath( aCore.EngineConfig(), kClusterBuildShaderPath );
    VkShaderModule        computeModule = aCore.CreateShaderModule( spvPath );

    const std::array< VkDescriptorSetLayoutBinding, 2 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myDescriptorSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: failed to create descriptor set layout" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( Gpu_ClusterBuildPushConstants );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.myDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: failed to create pipeline layout" );
    }

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = state.myPipelineLayout;
    if ( vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &state.myComputePipeline )
         != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: failed to create compute pipeline" );
    }
    UtilLogger::Info( "PIPELINE", "ClusterBuild compute pipeline created." );

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, computeModule, nullptr );

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: failed to create descriptor pool" );
    }

    const VkDevice              device          = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipeline            computePipeline = state.myComputePipeline;
    const VkPipelineLayout      pipelineLayout  = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout       = state.myDescriptorSetLayout;
    const VkDescriptorPool      descriptorPool  = state.myDescriptorPool;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, computePipeline, pipelineLayout, setLayout, descriptorPool ]() {
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

    const VkDeviceSize lightsBytes = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;
    aCore.CreateBuffer( lightsBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, state.myLightsBuffer, true );
    vmaMapMemory( aCore.myRhi.myDeviceCtx.myAllocator, state.myLightsBuffer.myAllocation, &state.myLightsMapped );

    const VmaAllocator       allocator    = aCore.myRhi.myDeviceCtx.myAllocator;
    const Vk_AllocatedBuffer lightsBuffer = state.myLightsBuffer;
    void* const              lightsMapped = state.myLightsMapped;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ allocator, lightsBuffer, lightsMapped ]() {
        if ( lightsMapped != nullptr ) {
            vmaUnmapMemory( allocator, lightsBuffer.myAllocation );
        }
        vmaDestroyBuffer( allocator, lightsBuffer.myBuffer, lightsBuffer.myAllocation );
    } );
}

}  // namespace

namespace Vk_ClusterBuildPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myClusterBuildState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroyClusterListBuffers( aCore, true );
    aCore.myClusterBuildState.myLightsMapped = nullptr;
    aCore.myClusterBuildState.myLightsBuffer = {};
    aCore.myClusterBuildState.myInitialized  = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myClusterBuildState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroyClusterListBuffers( aCore, false );
    AllocateClusterListBuffers( aCore, false );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myClusterBuildState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_ClusterBuildPass::Init." );
    CreatePipeline( aCore );
    AllocateClusterListBuffers( aCore, true );
    aCore.myClusterBuildState.myInitialized = true;
}

}  // namespace Vk_ClusterBuildPass

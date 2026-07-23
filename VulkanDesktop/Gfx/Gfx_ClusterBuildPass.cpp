#include "Gfx_ClusterBuildPass.h"

#include "Gfx_ClusterLighting.h"

#include <array>

namespace {

void FillDescriptorSets( Rhi_Device& aDevice, Gfx_ClusterBuildPass::PassState& aState, uint32_t aFramesInFlight ) {
    using namespace Gfx_ClusterBuildPass;
    const uint32_t frames      = aFramesInFlight > 0u ? aFramesInFlight : kMaxFramesInFlight;
    const uint64_t lightsRange = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;
    for ( uint32_t frame = 0; frame < frames && frame < kMaxFramesInFlight; ++frame ) {
        std::array< Rhi::DescriptorBufferWrite, 2 > writes{};
        writes[ 0 ].mySet         = aState.mySets[ frame ];
        writes[ 0 ].myBinding     = 0;
        writes[ 0 ].myType        = Rhi_DescriptorType::StorageBuffer;
        writes[ 0 ].myBuffer      = aState.myLightsBuffer;
        writes[ 0 ].myOffsetBytes = 0;
        writes[ 0 ].myRangeBytes  = lightsRange;

        writes[ 1 ].mySet        = aState.mySets[ frame ];
        writes[ 1 ].myBinding    = 1;
        writes[ 1 ].myType       = Rhi_DescriptorType::StorageBuffer;
        writes[ 1 ].myBuffer     = aState.myClusterListBuffers[ frame ];
        writes[ 1 ].myRangeBytes = 0;

        Rhi::DeviceUpdateDescriptorBuffers( aDevice, writes.data(), static_cast< uint32_t >( writes.size() ) );
    }
}

void DestroyListBuffersOnly( Rhi_Device& aDevice, Gfx_ClusterBuildPass::PassState& aState ) {
    for ( auto& buf : aState.myClusterListBuffers ) {
        if ( buf ) {
            Rhi::DeviceDestroyBuffer( aDevice, buf );
        }
    }
    aState.myClusterCount = 0;
    aState.myWidth        = 0;
    aState.myHeight       = 0;
    aState.myInitialized  = false;
}

}  // namespace

namespace Gfx_ClusterBuildPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.mySpirvCode == nullptr || aDesc.mySpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 2 > bindings = { {
        { 0, Rhi_DescriptorType::StorageBuffer, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageBuffer, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           setLayoutDesc{};
    setLayoutDesc.myBindings     = bindings.data();
    setLayoutDesc.myBindingCount = static_cast< uint32_t >( bindings.size() );
    aState.mySetLayout           = Rhi::DeviceCreateDescriptorSetLayout( aDevice, setLayoutDesc );
    if ( !aState.mySetLayout ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    const uint32_t                                 frames    = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const std::array< Rhi::DescriptorPoolSize, 1 > poolSizes = { {
        { Rhi_DescriptorType::StorageBuffer, frames * 2u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    aState.myPool            = Rhi::DeviceCreateDescriptorPool( aDevice, poolDesc );
    if ( !aState.myPool ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Compute;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = sizeof( Gpu_ClusterBuildPushConstants );

    Rhi::PipelineLayoutDesc layoutDesc{};
    layoutDesc.mySetLayouts     = &aState.mySetLayout;
    layoutDesc.mySetLayoutCount = 1;
    layoutDesc.myPushRanges     = &push;
    layoutDesc.myPushRangeCount = 1;
    aState.myLayout             = Rhi::DeviceCreatePipelineLayout( aDevice, layoutDesc );
    if ( !aState.myLayout ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( aDevice, aDesc.mySpirvCode, aDesc.mySpirvBytes );
    if ( !shader ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }
    Rhi::ComputePipelineDesc pipeDesc{};
    pipeDesc.myShader = shader;
    pipeDesc.myLayout = aState.myLayout;
    aState.myPipeline = Rhi::DeviceCreateComputePipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, shader );
    if ( !aState.myPipeline ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    const uint64_t  lightsBytes = sizeof( Gpu_ClusterLight ) * Gfx_ClusterLighting::kMaxLights;
    Rhi::BufferDesc lightsDesc{};
    lightsDesc.mySizeBytes = lightsBytes;
    lightsDesc.myUsage     = Rhi_BufferUsage::Storage;
    lightsDesc.myMemory    = Rhi_MemoryUsage::CpuToGpu;
    aState.myLightsBuffer  = Rhi::DeviceCreateBuffer( aDevice, lightsDesc );
    if ( !aState.myLightsBuffer ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }
    aState.myLightsMapped = Rhi::DeviceMapBuffer( aDevice, aState.myLightsBuffer );
    if ( aState.myLightsMapped == nullptr ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    aState.myPipelineReady = true;
    return true;
}

bool CreateOrRecreateLists( Rhi_Device& aDevice, const ListsInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myPipelineReady || aDesc.myWidth == 0 || aDesc.myHeight == 0 ) {
        return false;
    }

    const uint32_t clusterCount = Gfx_ClusterLighting::ClusterCount( aDesc.myWidth, aDesc.myHeight );
    const uint32_t frames       = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;

    if ( aState.myInitialized && aState.myWidth == aDesc.myWidth && aState.myHeight == aDesc.myHeight && aState.myClusterCount == clusterCount ) {
        FillDescriptorSets( aDevice, aState, frames );
        return true;
    }

    DestroyListBuffersOnly( aDevice, aState );

    const uint64_t  listBytes = static_cast< uint64_t >( clusterCount ) * sizeof( Gpu_ClusterLightList );
    Rhi::BufferDesc listDesc{};
    listDesc.mySizeBytes = listBytes;
    listDesc.myUsage     = Rhi_BufferUsage::Storage;
    listDesc.myMemory    = Rhi_MemoryUsage::GpuOnly;

    for ( uint32_t frame = 0; frame < frames && frame < kMaxFramesInFlight; ++frame ) {
        aState.myClusterListBuffers[ frame ] = Rhi::DeviceCreateBuffer( aDevice, listDesc );
        if ( !aState.myClusterListBuffers[ frame ] ) {
            DestroyLists( aDevice, aState );
            return false;
        }
    }

    if ( !aState.mySetsAllocated ) {
        for ( uint32_t frame = 0; frame < frames && frame < kMaxFramesInFlight; ++frame ) {
            aState.mySets[ frame ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.mySetLayout );
            if ( !aState.mySets[ frame ] ) {
                DestroyLists( aDevice, aState );
                return false;
            }
        }
        aState.mySetsAllocated = true;
    }

    aState.myClusterCount = clusterCount;
    aState.myWidth        = aDesc.myWidth;
    aState.myHeight       = aDesc.myHeight;

    FillDescriptorSets( aDevice, aState, frames );
    aState.myInitialized = true;
    return true;
}

void DestroyLists( Rhi_Device& aDevice, PassState& aState ) {
    DestroyListBuffersOnly( aDevice, aState );
}

void DestroyPipeline( Rhi_Device& aDevice, PassState& aState ) {
    for ( auto& set : aState.mySets ) {
        set = {};
    }
    aState.mySetsAllocated = false;

    if ( aState.myLightsBuffer ) {
        Rhi::DeviceUnmapBuffer( aDevice, aState.myLightsBuffer );
        Rhi::DeviceDestroyBuffer( aDevice, aState.myLightsBuffer );
    }
    aState.myLightsMapped = nullptr;

    if ( aState.myPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myPipeline );
    }
    if ( aState.myLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myLayout );
    }
    if ( aState.myPool ) {
        Rhi::DeviceDestroyDescriptorPool( aDevice, aState.myPool );
    }
    if ( aState.mySetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.mySetLayout );
    }
    aState.myPipelineReady = false;
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyLists( aDevice, aState );
    DestroyPipeline( aDevice, aState );
}

GpuResources MakeGpuResources( const PassState& aState, uint32_t aFrameIndex ) {
    GpuResources out{};
    if ( !aState.myInitialized || !aState.myPipelineReady ) {
        return out;
    }
    const uint32_t frame    = aFrameIndex % kMaxFramesInFlight;
    out.myPipeline          = aState.myPipeline;
    out.myLayout            = aState.myLayout;
    out.mySet               = aState.mySets[ frame ];
    out.myLightsBuffer      = aState.myLightsBuffer;
    out.myClusterListBuffer = aState.myClusterListBuffers[ frame ];
    return out;
}

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( aInput.clusterCount == 0 || !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    Rhi::BufferBarrier lightsToCompute{};
    lightsToCompute.myBuffer    = aGpu.myLightsBuffer;
    lightsToCompute.mySrcStage  = Rhi_PipelineStage::Host;
    lightsToCompute.myDstStage  = Rhi_PipelineStage::ComputeShader;
    lightsToCompute.mySrcAccess = Rhi_Access::HostWrite;
    lightsToCompute.myDstAccess = Rhi_Access::ShaderRead;
    Rhi::CommandListPipelineBarrier( aCmd, &lightsToCompute, 1 );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myLayout, 0, aGpu.mySet );

    Gpu_ClusterBuildPushConstants push{};
    push.clusterCount = aInput.clusterCount;
    push.lightCount   = aInput.lightCount;
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );

    const uint32_t groupCount = ( aInput.clusterCount + Gfx_ClusterLighting::kClusterBuildWorkgroupSize - 1 ) / Gfx_ClusterLighting::kClusterBuildWorkgroupSize;
    if ( groupCount > 0 ) {
        if ( aInput.debugLabels ) {
            Rhi::CommandListBeginDebugLabel( aCmd, "Pass=ClusterBuild" );
        }
        Rhi::CommandListDispatch( aCmd, groupCount, 1, 1 );
        if ( aInput.debugLabels ) {
            Rhi::CommandListEndDebugLabel( aCmd );
        }
    }

    Rhi::BufferBarrier listsBarrier{};
    listsBarrier.myBuffer    = aGpu.myClusterListBuffer;
    listsBarrier.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    listsBarrier.myDstStage  = Rhi_PipelineStage::FragmentShader;
    listsBarrier.mySrcAccess = Rhi_Access::ShaderWrite;
    listsBarrier.myDstAccess = Rhi_Access::ShaderRead;

    Rhi::BufferBarrier lightsToFragment{};
    lightsToFragment.myBuffer    = aGpu.myLightsBuffer;
    lightsToFragment.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    lightsToFragment.myDstStage  = Rhi_PipelineStage::FragmentShader;
    lightsToFragment.mySrcAccess = Rhi_Access::ShaderRead;
    lightsToFragment.myDstAccess = Rhi_Access::ShaderRead;

    const Rhi::BufferBarrier ssboBarriers[] = { listsBarrier, lightsToFragment };
    Rhi::CommandListPipelineBarrier( aCmd, ssboBarriers, 2 );
}

}  // namespace Gfx_ClusterBuildPass

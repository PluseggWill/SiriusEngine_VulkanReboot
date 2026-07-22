#include "Gfx_ClusterBuildPass.h"

#include "Gfx_ClusterLighting.h"

namespace Gfx_ClusterBuildPass {

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

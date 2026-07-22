#include "Gfx_DeferredLightingPass.h"

namespace Gfx_DeferredLightingPass {

void RecordDraw( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Fragment, 0, sizeof( aInput.myPush ), &aInput.myPush );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=DeferredLighting" );
    }
    Rhi::CommandListDraw( aCmd, 3, 1, 0, 0 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

}  // namespace Gfx_DeferredLightingPass

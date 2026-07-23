#include "Gfx_DeferredLightingPass.h"

namespace Gfx_DeferredLightingPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady && aState.myPipeline ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myVertSpirv == nullptr || aDesc.myVertSpirvBytes == 0 || aDesc.myFragSpirv == nullptr || aDesc.myFragSpirvBytes == 0
         || !aDesc.myLayout || !aDesc.myRenderPass ) {
        return false;
    }

    Rhi_ShaderModule vert = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myVertSpirv, aDesc.myVertSpirvBytes );
    Rhi_ShaderModule frag = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myFragSpirv, aDesc.myFragSpirvBytes );
    if ( !vert || !frag ) {
        Rhi::DeviceDestroyShaderModule( aDevice, vert );
        Rhi::DeviceDestroyShaderModule( aDevice, frag );
        return false;
    }

    Rhi::GraphicsPipelineDesc pipeDesc{};
    pipeDesc.myVertexShader           = vert;
    pipeDesc.myFragmentShader         = frag;
    pipeDesc.myLayout                 = aDesc.myLayout;
    pipeDesc.myRenderPass             = aDesc.myRenderPass;
    pipeDesc.myColorAttachmentCount   = 1;
    pipeDesc.mySampleCount            = aDesc.mySampleCount > 0 ? aDesc.mySampleCount : 1u;
    pipeDesc.myCullMode               = Rhi_CullMode::None;
    pipeDesc.myDepthTestEnable        = false;
    pipeDesc.myDepthWriteEnable       = false;
    pipeDesc.myBlendEnable            = false;
    pipeDesc.myDynamicViewportScissor = true;

    aState.myPipeline = Rhi::DeviceCreateGraphicsPipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, vert );
    Rhi::DeviceDestroyShaderModule( aDevice, frag );
    if ( !aState.myPipeline ) {
        return false;
    }

    aState.myPipelineReady = true;
    return true;
}

void DestroyPipeline( Rhi_Device& aDevice, PassState& aState ) {
    if ( aState.myPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myPipeline );
    }
    aState.myPipelineReady = false;
}

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

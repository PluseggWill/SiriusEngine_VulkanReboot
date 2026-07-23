#include "Gfx_DdgiPass.h"

namespace {

void BarrierAtlas( Rhi_CommandList& aCmd, Rhi_Texture aTex, Rhi_ImageLayout aOld, Rhi_ImageLayout aNew, Rhi_Access aSrc, Rhi_Access aDst, Rhi_PipelineStage aSrcStage,
                   Rhi_PipelineStage aDstStage ) {
    Rhi::ImageBarrier b{};
    b.myTexture   = aTex;
    b.myOldLayout = aOld;
    b.myNewLayout = aNew;
    b.mySrcAccess = aSrc;
    b.myDstAccess = aDst;
    b.mySrcStage  = aSrcStage;
    b.myDstStage  = aDstStage;
    Rhi::CommandListPipelineBarrier( aCmd, &b, 1 );
}

}  // namespace

namespace Gfx_DdgiPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady && aState.myPipeline ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.mySpirvCode == nullptr || aDesc.mySpirvBytes == 0 || !aDesc.myLayout ) {
        return false;
    }

    Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( aDevice, aDesc.mySpirvCode, aDesc.mySpirvBytes );
    if ( !shader ) {
        return false;
    }

    Rhi::ComputePipelineDesc pipeDesc{};
    pipeDesc.myShader = shader;
    pipeDesc.myLayout = aDesc.myLayout;
    aState.myPipeline = Rhi::DeviceCreateComputePipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, shader );
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

void RecordProbeUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( aInput.myAtlasWidth == 0 || aInput.myAtlasHeight == 0 || !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    BarrierAtlas( aCmd, aGpu.myIrradianceAtlas, Rhi_ImageLayout::ShaderReadOnly, Rhi_ImageLayout::General, Rhi_Access::ShaderRead,
                  Rhi_Access::ShaderRead | Rhi_Access::ShaderWrite, Rhi_PipelineStage::FragmentShader, Rhi_PipelineStage::ComputeShader );
    BarrierAtlas( aCmd, aGpu.myVisibilityAtlas, Rhi_ImageLayout::ShaderReadOnly, Rhi_ImageLayout::General, Rhi_Access::ShaderRead,
                  Rhi_Access::ShaderRead | Rhi_Access::ShaderWrite, Rhi_PipelineStage::FragmentShader, Rhi_PipelineStage::ComputeShader );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Compute, 0, sizeof( aInput.myPush ), &aInput.myPush );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=DDGI ProbeUpdate" );
    }
    Rhi::CommandListDispatch( aCmd, ( aInput.myAtlasWidth + 7 ) / 8, ( aInput.myAtlasHeight + 7 ) / 8, 1 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }

    BarrierAtlas( aCmd, aGpu.myIrradianceAtlas, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
    BarrierAtlas( aCmd, aGpu.myVisibilityAtlas, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
}

}  // namespace Gfx_DdgiPass

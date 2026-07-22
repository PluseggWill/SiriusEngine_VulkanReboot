#include "Gfx_SsrPass.h"

namespace {

void BarrierColor( Rhi_CommandList& aCmd, Rhi_Texture aTex, Rhi_ImageLayout aOld, Rhi_ImageLayout aNew, Rhi_Access aSrcAccess, Rhi_Access aDstAccess,
                   Rhi_PipelineStage aSrcStage, Rhi_PipelineStage aDstStage ) {
    Rhi::ImageBarrier b{};
    b.myTexture   = aTex;
    b.myOldLayout = aOld;
    b.myNewLayout = aNew;
    b.mySrcAccess = aSrcAccess;
    b.myDstAccess = aDstAccess;
    b.mySrcStage  = aSrcStage;
    b.myDstStage  = aDstStage;
    Rhi::CommandListPipelineBarrier( aCmd, &b, 1 );
}

}  // namespace

namespace Gfx_SsrPass {

void RecordTrace( Rhi_CommandList& aCmd, const GpuResources& aGpu, TraceInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.myOutputLayout == nullptr || !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    const Rhi_ImageLayout   oldLayout = *aInput.myOutputLayout;
    const Rhi_Access        srcAccess = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_Access::None : Rhi_Access::ShaderRead;
    const Rhi_PipelineStage srcStage  = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_PipelineStage::TopOfPipe : Rhi_PipelineStage::FragmentShader;
    BarrierColor( aCmd, aGpu.myOutput, oldLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader );
    *aInput.myOutputLayout = Rhi_ImageLayout::General;

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Compute, 0, sizeof( aInput.myPush ), &aInput.myPush );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=SSR" );
    }
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }

    BarrierColor( aCmd, aGpu.myOutput, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
    *aInput.myOutputLayout = Rhi_ImageLayout::ShaderReadOnly;
}

void RecordHistoryUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, HistoryInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.mySceneLayout == nullptr || aInput.myHistoryLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    BarrierColor( aCmd, aGpu.mySceneColor, Rhi_ImageLayout::ShaderReadOnly, Rhi_ImageLayout::TransferSrc, Rhi_Access::ShaderRead, Rhi_Access::TransferRead,
                  Rhi_PipelineStage::FragmentShader, Rhi_PipelineStage::Transfer );
    *aInput.mySceneLayout = Rhi_ImageLayout::TransferSrc;

    const Rhi_ImageLayout   histOld   = *aInput.myHistoryLayout;
    const Rhi_Access        histSrc   = histOld == Rhi_ImageLayout::Undefined ? Rhi_Access::None : Rhi_Access::ShaderRead;
    const Rhi_PipelineStage histStage = histOld == Rhi_ImageLayout::Undefined ? Rhi_PipelineStage::TopOfPipe : Rhi_PipelineStage::FragmentShader;
    BarrierColor( aCmd, aGpu.myHistoryWrite, histOld, Rhi_ImageLayout::TransferDst, histSrc, Rhi_Access::TransferWrite, histStage, Rhi_PipelineStage::Transfer );
    *aInput.myHistoryLayout = Rhi_ImageLayout::TransferDst;

    Rhi::ImageCopy copy{};
    copy.mySrc       = aGpu.mySceneColor;
    copy.myDst       = aGpu.myHistoryWrite;
    copy.mySrcLayout = Rhi_ImageLayout::TransferSrc;
    copy.myDstLayout = Rhi_ImageLayout::TransferDst;
    copy.myWidth     = aInput.myWidth;
    copy.myHeight    = aInput.myHeight;
    Rhi::CommandListCopyImage( aCmd, copy );

    BarrierColor( aCmd, aGpu.mySceneColor, Rhi_ImageLayout::TransferSrc, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::TransferRead, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::Transfer, Rhi_PipelineStage::FragmentShader );
    BarrierColor( aCmd, aGpu.myHistoryWrite, Rhi_ImageLayout::TransferDst, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::TransferWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::Transfer, Rhi_PipelineStage::FragmentShader );
    *aInput.mySceneLayout   = Rhi_ImageLayout::ShaderReadOnly;
    *aInput.myHistoryLayout = Rhi_ImageLayout::ShaderReadOnly;
}

}  // namespace Gfx_SsrPass

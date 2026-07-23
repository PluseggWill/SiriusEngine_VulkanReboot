#include "Gfx_PostProcessPass.h"

#include "../Rhi/Rhi_Device.h"

namespace {

bool CreateOneCompute( Rhi_Device& aDevice, const void* aSpirv, size_t aBytes, Rhi_PipelineLayout aLayout, Rhi_Pipeline& aOut ) {
    if ( aSpirv == nullptr || aBytes == 0 || !aLayout ) {
        return false;
    }
    Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( aDevice, aSpirv, aBytes );
    if ( !shader ) {
        return false;
    }
    Rhi::ComputePipelineDesc desc{};
    desc.myShader = shader;
    desc.myLayout = aLayout;
    aOut          = Rhi::DeviceCreateComputePipeline( aDevice, desc );
    Rhi::DeviceDestroyShaderModule( aDevice, shader );
    return static_cast< bool >( aOut );
}

}  // namespace

namespace Gfx_PostProcessPass {

bool CreateComputePipelines( Rhi_Device& aDevice, const ComputePipelinesInitDesc& aDesc, ComputePassState& aState ) {
    if ( aState.myPipelinesReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) ) {
        return false;
    }
    if ( !CreateOneCompute( aDevice, aDesc.myThresholdSpirv, aDesc.myThresholdSpirvBytes, aDesc.myThresholdLayout, aState.myThresholdPipeline )
         || !CreateOneCompute( aDevice, aDesc.myBlurSpirv, aDesc.myBlurSpirvBytes, aDesc.myBlurLayout, aState.myBlurPipeline )
         || !CreateOneCompute( aDevice, aDesc.myTaaSpirv, aDesc.myTaaSpirvBytes, aDesc.myTaaLayout, aState.myTaaPipeline ) ) {
        DestroyComputePipelines( aDevice, aState );
        return false;
    }
    aState.myPipelinesReady = true;
    return true;
}

void DestroyComputePipelines( Rhi_Device& aDevice, ComputePassState& aState ) {
    if ( aState.myThresholdPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myThresholdPipeline );
    }
    if ( aState.myBlurPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myBlurPipeline );
    }
    if ( aState.myTaaPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myTaaPipeline );
    }
    aState.myPipelinesReady = false;
}

}  // namespace Gfx_PostProcessPass

namespace {

void Barrier( Rhi_CommandList& aCmd, Rhi_Texture aTex, Rhi_ImageLayout aOld, Rhi_ImageLayout aNew, Rhi_Access aSrcAccess, Rhi_Access aDstAccess, Rhi_PipelineStage aSrcStage,
              Rhi_PipelineStage aDstStage ) {
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

void EnsureSceneShaderRead( Rhi_CommandList& aCmd, Rhi_Texture aScene, Rhi_ImageLayout& aLayout, Rhi_PipelineStage aDstStage ) {
    if ( aLayout == Rhi_ImageLayout::ShaderReadOnly ) {
        return;
    }
    if ( aLayout != Rhi_ImageLayout::Undefined ) {
        Barrier( aCmd, aScene, aLayout, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ColorAttachmentRW, Rhi_Access::ShaderRead, Rhi_PipelineStage::ColorAttachment,
                 aDstStage );
    }
    aLayout = Rhi_ImageLayout::ShaderReadOnly;
}

void EnsureGeneralForWrite( Rhi_CommandList& aCmd, Rhi_Texture aTex, Rhi_ImageLayout& aLayout ) {
    if ( aLayout == Rhi_ImageLayout::General ) {
        return;
    }
    Rhi_Access        srcAccess = Rhi_Access::None;
    Rhi_PipelineStage srcStage  = Rhi_PipelineStage::TopOfPipe;
    if ( aLayout == Rhi_ImageLayout::ShaderReadOnly ) {
        srcAccess = Rhi_Access::ShaderRead;
        srcStage  = Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader;
    }
    else if ( aLayout != Rhi_ImageLayout::Undefined ) {
        srcAccess = Rhi_Access::ShaderWrite;
        srcStage  = Rhi_PipelineStage::ComputeShader;
    }
    Barrier( aCmd, aTex, aLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader );
    aLayout = Rhi_ImageLayout::General;
}

}  // namespace

namespace Gfx_PostProcessPass {

void RecordBloom( Rhi_CommandList& aCmd, const BloomGpu& aGpu, BloomInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.mySceneLayout == nullptr || aInput.myPingLayout == nullptr || aInput.myPongLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    EnsureSceneShaderRead( aCmd, aGpu.mySceneColor, *aInput.mySceneLayout, Rhi_PipelineStage::ComputeShader );
    EnsureGeneralForWrite( aCmd, aGpu.myBloomPing, *aInput.myPingLayout );
    EnsureGeneralForWrite( aCmd, aGpu.myBloomPong, *aInput.myPongLayout );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myThresholdPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myThresholdLayout, 0, aGpu.myThresholdSet );
    const float thresholdPush[ 4 ] = { aInput.myThreshold, 0.f, 0.f, 0.f };
    Rhi::CommandListPushConstants( aCmd, aGpu.myThresholdLayout, Rhi_ShaderStage::Compute, 0, sizeof( thresholdPush ), thresholdPush );
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );

    Rhi::MemoryBarrierDesc afterThreshold{};
    afterThreshold.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    afterThreshold.myDstStage  = Rhi_PipelineStage::ComputeShader;
    afterThreshold.mySrcAccess = Rhi_Access::ShaderWrite;
    afterThreshold.myDstAccess = Rhi_Access::ShaderRead;
    Rhi::CommandListMemoryBarrier( aCmd, afterThreshold );

    auto runBlur = [ & ]( Rhi_DescriptorSet aSet, uint32_t aAxisX, uint32_t aAxisY ) {
        Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurPipeline );
        Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurLayout, 0, aSet );
        const uint32_t blurPush[ 2 ] = { aAxisX, aAxisY };
        Rhi::CommandListPushConstants( aCmd, aGpu.myBlurLayout, Rhi_ShaderStage::Compute, 0, sizeof( blurPush ), blurPush );
        Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    };

    runBlur( aGpu.myBlurHorizSet, 1u, 0u );

    Rhi::MemoryBarrierDesc blurDep{};
    blurDep.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    blurDep.myDstStage  = Rhi_PipelineStage::ComputeShader;
    blurDep.mySrcAccess = Rhi_Access::ShaderWrite;
    blurDep.myDstAccess = Rhi_Access::ShaderRead;
    Rhi::CommandListMemoryBarrier( aCmd, blurDep );

    runBlur( aGpu.myBlurVertSet, 0u, 1u );

    Barrier( aCmd, aGpu.myBloomPing, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
             Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
    *aInput.myPingLayout = Rhi_ImageLayout::ShaderReadOnly;
}

void RecordTaa( Rhi_CommandList& aCmd, const TaaGpu& aGpu, TaaInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.mySceneLayout == nullptr || aInput.myResolvedLayout == nullptr || aInput.myHistoryReadLayout == nullptr
         || aInput.myHistoryWriteLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    EnsureSceneShaderRead( aCmd, aGpu.mySceneColor, *aInput.mySceneLayout, Rhi_PipelineStage::ComputeShader );

    if ( *aInput.myHistoryReadLayout != Rhi_ImageLayout::ShaderReadOnly ) {
        Rhi_Access        srcAccess = Rhi_Access::None;
        Rhi_PipelineStage srcStage  = Rhi_PipelineStage::TopOfPipe;
        if ( *aInput.myHistoryReadLayout == Rhi_ImageLayout::General ) {
            srcAccess = Rhi_Access::ShaderWrite;
            srcStage  = Rhi_PipelineStage::ComputeShader;
        }
        Barrier( aCmd, aGpu.myHistoryRead, *aInput.myHistoryReadLayout, Rhi_ImageLayout::ShaderReadOnly, srcAccess, Rhi_Access::ShaderRead, srcStage,
                 Rhi_PipelineStage::ComputeShader );
        *aInput.myHistoryReadLayout = Rhi_ImageLayout::ShaderReadOnly;
    }

    if ( *aInput.myResolvedLayout != Rhi_ImageLayout::General ) {
        Rhi_Access        srcAccess = Rhi_Access::None;
        Rhi_PipelineStage srcStage  = Rhi_PipelineStage::TopOfPipe;
        if ( *aInput.myResolvedLayout == Rhi_ImageLayout::ShaderReadOnly ) {
            srcAccess = Rhi_Access::ShaderRead;
            srcStage  = Rhi_PipelineStage::FragmentShader;
        }
        else if ( *aInput.myResolvedLayout == Rhi_ImageLayout::General ) {
            srcAccess = Rhi_Access::ShaderWrite;
            srcStage  = Rhi_PipelineStage::ComputeShader;
        }
        Barrier( aCmd, aGpu.myResolved, *aInput.myResolvedLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader );
        *aInput.myResolvedLayout = Rhi_ImageLayout::General;
    }

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Compute, 0, sizeof( aInput.myPush ), &aInput.myPush );
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );

    Rhi::MemoryBarrierDesc dep{};
    dep.mySrcStage  = Rhi_PipelineStage::ComputeShader;
    dep.myDstStage  = Rhi_PipelineStage::ComputeShader | Rhi_PipelineStage::FragmentShader;
    dep.mySrcAccess = Rhi_Access::ShaderWrite;
    dep.myDstAccess = Rhi_Access::ShaderRead;
    Rhi::CommandListMemoryBarrier( aCmd, dep );

    Barrier( aCmd, aGpu.myResolved, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
             Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
    *aInput.myResolvedLayout = Rhi_ImageLayout::ShaderReadOnly;

    if ( *aInput.myHistoryWriteLayout != Rhi_ImageLayout::General ) {
        Rhi_Access        srcAccess = Rhi_Access::None;
        Rhi_PipelineStage srcStage  = Rhi_PipelineStage::TopOfPipe;
        if ( *aInput.myHistoryWriteLayout == Rhi_ImageLayout::ShaderReadOnly ) {
            srcAccess = Rhi_Access::ShaderRead;
            srcStage  = Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader;
        }
        Barrier( aCmd, aGpu.myHistoryWrite, *aInput.myHistoryWriteLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage,
                 Rhi_PipelineStage::ComputeShader );
        *aInput.myHistoryWriteLayout = Rhi_ImageLayout::General;
    }

    Rhi::ImageBarrier toCopy[ 2 ]{};
    toCopy[ 0 ].myTexture   = aGpu.myResolved;
    toCopy[ 0 ].myOldLayout = Rhi_ImageLayout::ShaderReadOnly;
    toCopy[ 0 ].myNewLayout = Rhi_ImageLayout::TransferSrc;
    toCopy[ 0 ].mySrcAccess = Rhi_Access::ShaderRead;
    toCopy[ 0 ].myDstAccess = Rhi_Access::TransferRead;
    toCopy[ 0 ].mySrcStage  = Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader;
    toCopy[ 0 ].myDstStage  = Rhi_PipelineStage::Transfer;

    toCopy[ 1 ].myTexture   = aGpu.myHistoryWrite;
    toCopy[ 1 ].myOldLayout = Rhi_ImageLayout::General;
    toCopy[ 1 ].myNewLayout = Rhi_ImageLayout::TransferDst;
    toCopy[ 1 ].mySrcAccess = Rhi_Access::ShaderWrite;
    toCopy[ 1 ].myDstAccess = Rhi_Access::TransferWrite;
    toCopy[ 1 ].mySrcStage  = Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader;
    toCopy[ 1 ].myDstStage  = Rhi_PipelineStage::Transfer;
    Rhi::CommandListPipelineBarrier( aCmd, toCopy, 2 );

    Rhi::ImageCopy copy{};
    copy.mySrc       = aGpu.myResolved;
    copy.myDst       = aGpu.myHistoryWrite;
    copy.mySrcLayout = Rhi_ImageLayout::TransferSrc;
    copy.myDstLayout = Rhi_ImageLayout::TransferDst;
    copy.myWidth     = aInput.myWidth;
    copy.myHeight    = aInput.myHeight;
    Rhi::CommandListCopyImage( aCmd, copy );

    Rhi::ImageBarrier toRead[ 2 ]{};
    toRead[ 0 ].myTexture   = aGpu.myResolved;
    toRead[ 0 ].myOldLayout = Rhi_ImageLayout::TransferSrc;
    toRead[ 0 ].myNewLayout = Rhi_ImageLayout::ShaderReadOnly;
    toRead[ 0 ].mySrcAccess = Rhi_Access::TransferRead;
    toRead[ 0 ].myDstAccess = Rhi_Access::ShaderRead;
    toRead[ 0 ].mySrcStage  = Rhi_PipelineStage::Transfer;
    toRead[ 0 ].myDstStage  = Rhi_PipelineStage::ComputeShader | Rhi_PipelineStage::FragmentShader;

    toRead[ 1 ].myTexture   = aGpu.myHistoryWrite;
    toRead[ 1 ].myOldLayout = Rhi_ImageLayout::TransferDst;
    toRead[ 1 ].myNewLayout = Rhi_ImageLayout::ShaderReadOnly;
    toRead[ 1 ].mySrcAccess = Rhi_Access::TransferWrite;
    toRead[ 1 ].myDstAccess = Rhi_Access::ShaderRead;
    toRead[ 1 ].mySrcStage  = Rhi_PipelineStage::Transfer;
    toRead[ 1 ].myDstStage  = Rhi_PipelineStage::ComputeShader | Rhi_PipelineStage::FragmentShader;
    Rhi::CommandListPipelineBarrier( aCmd, toRead, 2 );

    *aInput.myResolvedLayout     = Rhi_ImageLayout::ShaderReadOnly;
    *aInput.myHistoryWriteLayout = Rhi_ImageLayout::ShaderReadOnly;
}

void RecordTonemap( Rhi_CommandList& aCmd, const TonemapGpu& aGpu, TonemapInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.mySceneLayout == nullptr || aInput.myPingLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    if ( !aInput.myBloomEnabled && *aInput.myPingLayout != Rhi_ImageLayout::ShaderReadOnly ) {
        const Rhi_ImageLayout   oldLayout = *aInput.myPingLayout;
        const Rhi_PipelineStage srcStage  = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_PipelineStage::TopOfPipe : Rhi_PipelineStage::ComputeShader;
        Barrier( aCmd, aGpu.myBloomPing, oldLayout, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::None, Rhi_Access::ShaderRead, srcStage, Rhi_PipelineStage::FragmentShader );
        *aInput.myPingLayout = Rhi_ImageLayout::ShaderReadOnly;
    }

    if ( *aInput.mySceneLayout != Rhi_ImageLayout::ShaderReadOnly ) {
        if ( *aInput.mySceneLayout != Rhi_ImageLayout::Undefined ) {
            Barrier( aCmd, aGpu.mySceneColor, *aInput.mySceneLayout, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ColorAttachmentRW, Rhi_Access::ShaderRead,
                     Rhi_PipelineStage::ColorAttachment, Rhi_PipelineStage::FragmentShader );
        }
        *aInput.mySceneLayout = Rhi_ImageLayout::ShaderReadOnly;
    }

    const Rhi::ClearValue clears[ 2 ] = {
        Rhi::MakeClearColor( 0.0f, 0.0f, 0.0f, 1.0f ),
        Rhi::MakeClearDepthStencil( 1.0f, 0 ),
    };

    Rhi::RenderPassBeginInfo begin{};
    begin.myRenderPass  = aGpu.myRenderPass;
    begin.myFramebuffer = aGpu.myFramebuffer;
    begin.myWidth       = aInput.myWidth;
    begin.myHeight      = aInput.myHeight;
    begin.myClears      = clears;
    begin.myClearCount  = 2;
    Rhi::CommandListBeginRenderPass( aCmd, begin );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Fragment, 0, sizeof( aInput.myPush ), &aInput.myPush );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=Tonemap" );
    }
    Rhi::CommandListDraw( aCmd, 3, 1, 0, 0 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }

    Rhi::CommandListEndRenderPass( aCmd );
}

}  // namespace Gfx_PostProcessPass

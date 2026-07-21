#include "Gfx_AoPass.h"

#include "Gfx_AoMethod.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>

namespace {

struct ClassicAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 viewProj;
    alignas( 16 ) glm::vec4 params;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( ClassicAoPushConstants ) == 224, "ClassicAoPushConstants must match Ssao.comp push block" );

struct HalfResAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::vec4 params;
    alignas( 8 ) glm::uvec2 sliceCount;
    alignas( 8 ) glm::uvec2 stepsPerSlice;
    alignas( 8 ) glm::vec2 halfScreenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( HalfResAoPushConstants ) == 176, "HalfResAoPushConstants must match half-res AO compute shaders" );

struct AoUpsamplePushConstants {
    alignas( 8 ) glm::vec2 fullScreenSize;
    float depthSigma;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoUpsamplePushConstants ) == 24, "AoUpsamplePushConstants must match AoUpsample.comp push block" );

struct AoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( AoBlurPushConstants ) == 8, "AoBlurPushConstants must match SsaoBlur.comp push block" );

struct AoTemporalPushConstants {
    alignas( 4 ) float historyBlend;
    alignas( 4 ) float historyValid;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoTemporalPushConstants ) == 16, "AoTemporalPushConstants must match AoTemporal.comp push block" );

uint32_t HalfDim( uint32_t aValue ) {
    return std::max( 1u, ( aValue + 1u ) / 2u );
}

void MaybeLabelBegin( Rhi_CommandList& aCmd, bool aEnabled, const char* aName ) {
    if ( aEnabled ) {
        Rhi::CommandListBeginDebugLabel( aCmd, aName );
    }
}

void MaybeLabelEnd( Rhi_CommandList& aCmd, bool aEnabled ) {
    if ( aEnabled ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

void BarrierColor( Rhi_CommandList& aCmd, Rhi_Texture aTexture, Rhi_ImageLayout aOldLayout, Rhi_ImageLayout aNewLayout, Rhi_Access aSrcAccess, Rhi_Access aDstAccess,
                   Rhi_PipelineStage aSrcStage, Rhi_PipelineStage aDstStage ) {
    Rhi::ImageBarrier barrier{};
    barrier.myTexture   = aTexture;
    barrier.myOldLayout = aOldLayout;
    barrier.myNewLayout = aNewLayout;
    barrier.mySrcAccess = aSrcAccess;
    barrier.myDstAccess = aDstAccess;
    barrier.mySrcStage  = aSrcStage;
    barrier.myDstStage  = aDstStage;
    Rhi::CommandListPipelineBarrier( aCmd, &barrier, 1 );
}

void BarrierForComputeWrite( Rhi_CommandList& aCmd, Rhi_Texture aTexture, Rhi_ImageLayout& aInOutLayout ) {
    const Rhi_ImageLayout   oldLayout = aInOutLayout;
    const Rhi_Access        srcAccess = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_Access::None : Rhi_Access::ShaderRead;
    const Rhi_PipelineStage srcStage  = oldLayout == Rhi_ImageLayout::Undefined        ? Rhi_PipelineStage::TopOfPipe
                                        : oldLayout == Rhi_ImageLayout::ShaderReadOnly ? Rhi_PipelineStage::FragmentShader
                                                                                       : Rhi_PipelineStage::ComputeShader;
    BarrierColor( aCmd, aTexture, oldLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader );
    aInOutLayout = Rhi_ImageLayout::General;
}

void BarrierGBufferForAoRead( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu ) {
    std::array< Rhi::ImageBarrier, 2 > barriers{};
    barriers[ 0 ].myTexture   = aGpu.myGBufferNormal;
    barriers[ 0 ].myOldLayout = Rhi_ImageLayout::ShaderReadOnly;
    barriers[ 0 ].myNewLayout = Rhi_ImageLayout::ShaderReadOnly;
    barriers[ 0 ].mySrcAccess = Rhi_Access::ColorAttachmentRW;
    barriers[ 0 ].myDstAccess = Rhi_Access::ShaderRead;
    barriers[ 0 ].mySrcStage  = Rhi_PipelineStage::ColorAttachment;
    barriers[ 0 ].myDstStage  = Rhi_PipelineStage::ComputeShader;
    barriers[ 1 ]             = barriers[ 0 ];
    barriers[ 1 ].myTexture   = aGpu.myGBufferWorldPos;
    Rhi::CommandListPipelineBarrier( aCmd, barriers.data(), static_cast< uint32_t >( barriers.size() ) );
}

void DispatchBlur( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Rhi_DescriptorSet aSet, uint32_t aAxisX, uint32_t aAxisY, uint32_t aWidth, uint32_t aHeight,
                   bool aDebugLabels, const char* aDebugLabel ) {
    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurLayout, 0, aSet );
    AoBlurPushConstants push{};
    push.axisX = aAxisX;
    push.axisY = aAxisY;
    Rhi::CommandListPushConstants( aCmd, aGpu.myBlurLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );
    MaybeLabelBegin( aCmd, aDebugLabels, aDebugLabel );
    Rhi::CommandListDispatch( aCmd, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    MaybeLabelEnd( aCmd, aDebugLabels );
}

void RecordClassicSsao( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myClassicPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myClassicLayout, 0, aGpu.myClassicSet );

    ClassicAoPushConstants push{};
    push.view       = aInput.myView;
    push.proj       = aInput.myProj;
    push.viewProj   = aInput.myProj * aInput.myView;
    push.params     = glm::vec4( aInput.mySettings.myRadius, aInput.mySettings.myBias, aInput.mySettings.myNormalAwareRadius, aInput.mySettings.myEnabled ? 1.0f : 0.0f );
    push.screenSize = glm::vec2( static_cast< float >( aInput.myWidth ), static_cast< float >( aInput.myHeight ) );
    Rhi::CommandListPushConstants( aCmd, aGpu.myClassicLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );

    MaybeLabelBegin( aCmd, aInput.myDebugLabels, "Pass=AO ClassicSSAO" );
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    MaybeLabelEnd( aCmd, aInput.myDebugLabels );
}

void RecordHalfResUpsample( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    BarrierForComputeWrite( aCmd, aGpu.myAoRaw, aInput.myLayouts->myAoRaw );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myUpsamplePipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myUpsampleLayout, 0, aGpu.myUpsampleSet );

    AoUpsamplePushConstants upPush{};
    upPush.fullScreenSize = glm::vec2( static_cast< float >( aInput.myWidth ), static_cast< float >( aInput.myHeight ) );
    upPush.depthSigma     = aInput.mySettings.myUpsampleDepthSigma;
    Rhi::CommandListPushConstants( aCmd, aGpu.myUpsampleLayout, Rhi_ShaderStage::Compute, 0, sizeof( upPush ), &upPush );

    MaybeLabelBegin( aCmd, aInput.myDebugLabels, "Pass=AO Upsample" );
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    MaybeLabelEnd( aCmd, aInput.myDebugLabels );
}

void RecordHalfResCompute( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput, Rhi_Pipeline aPipeline,
                           const HalfResAoPushConstants& aPush, const char* aDebugLabel, bool aWritesBentNormal ) {
    const uint32_t halfW = HalfDim( aInput.myWidth );
    const uint32_t halfH = HalfDim( aInput.myHeight );

    BarrierForComputeWrite( aCmd, aGpu.myAoHalf, aInput.myLayouts->myAoHalf );
    if ( aWritesBentNormal ) {
        BarrierForComputeWrite( aCmd, aGpu.myBentNormalHalf, aInput.myLayouts->myBentHalf );
    }

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myHalfResLayout, 0, aGpu.myHalfResSet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myHalfResLayout, Rhi_ShaderStage::Compute, 0, sizeof( aPush ), &aPush );

    MaybeLabelBegin( aCmd, aInput.myDebugLabels, aDebugLabel );
    Rhi::CommandListDispatch( aCmd, ( halfW + 7 ) / 8, ( halfH + 7 ) / 8, 1 );
    MaybeLabelEnd( aCmd, aInput.myDebugLabels );

    BarrierColor( aCmd, aGpu.myAoHalf, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader );
    aInput.myLayouts->myAoHalf = Rhi_ImageLayout::ShaderReadOnly;

    if ( aWritesBentNormal ) {
        BarrierColor( aCmd, aGpu.myBentNormalHalf, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                      Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader );
        aInput.myLayouts->myBentHalf = Rhi_ImageLayout::ShaderReadOnly;
    }
}

void RecordHbaoPlus( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    const uint32_t halfW = HalfDim( aInput.myWidth );
    const uint32_t halfH = HalfDim( aInput.myHeight );

    HalfResAoPushConstants push{};
    push.view           = aInput.myView;
    push.proj           = aInput.myProj;
    push.params         = glm::vec4( aInput.mySettings.myRadius, aInput.mySettings.myBias, aInput.mySettings.myEnabled ? 1.0f : 0.0f, aInput.mySettings.myNormalAwareRadius );
    push.sliceCount     = glm::uvec2( std::clamp( aInput.mySettings.myHbaoDirections, 1u, 8u ), 0u );
    push.stepsPerSlice  = glm::uvec2( std::clamp( aInput.mySettings.myHbaoSteps, 1u, 8u ), 0u );
    push.halfScreenSize = glm::vec2( static_cast< float >( halfW ), static_cast< float >( halfH ) );

    RecordHalfResCompute( aCmd, aGpu, aInput, aGpu.myHbaoPipeline, push, "Pass=AO HBAO+", false );
    RecordHalfResUpsample( aCmd, aGpu, aInput );
}

void RecordGtao( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    const uint32_t halfW = HalfDim( aInput.myWidth );
    const uint32_t halfH = HalfDim( aInput.myHeight );

    HalfResAoPushConstants push{};
    push.view   = aInput.myView;
    push.proj   = aInput.myProj;
    push.params = glm::vec4( aInput.mySettings.myRadius, aInput.mySettings.myNormalAwareRadius, aInput.mySettings.myEnabled ? 1.0f : 0.0f, aInput.mySettings.myGtaoFalloff );
    push.sliceCount     = glm::uvec2( std::clamp( aInput.mySettings.myGtaoSlices, 1u, 16u ), 0u );
    push.stepsPerSlice  = glm::uvec2( std::clamp( aInput.mySettings.myGtaoStepsPerSlice, 1u, 16u ), 0u );
    push.halfScreenSize = glm::vec2( static_cast< float >( halfW ), static_cast< float >( halfH ) );

    RecordHalfResCompute( aCmd, aGpu, aInput, aGpu.myGtaoPipeline, push, "Pass=AO GTAO", true );
    RecordHalfResUpsample( aCmd, aGpu, aInput );
}

void RecordClassicBlur( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    BarrierColor( aCmd, aGpu.myAoRaw, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead, Rhi_PipelineStage::ComputeShader,
                  Rhi_PipelineStage::ComputeShader );

    BarrierForComputeWrite( aCmd, aGpu.myAoBlur, aInput.myLayouts->myAoBlur );
    DispatchBlur( aCmd, aGpu, aGpu.myBlurHorizSet, 1, 0, aInput.myWidth, aInput.myHeight, aInput.myDebugLabels, "Pass=AO BlurH" );

    BarrierColor( aCmd, aGpu.myAoBlur, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead, Rhi_PipelineStage::ComputeShader,
                  Rhi_PipelineStage::ComputeShader );
    DispatchBlur( aCmd, aGpu, aGpu.myBlurVertSet, 0, 1, aInput.myWidth, aInput.myHeight, aInput.myDebugLabels, "Pass=AO BlurV" );
}

void RecordTemporalFilter( Rhi_CommandList& aCmd, const Gfx_AoPass::GpuResources& aGpu, Gfx_AoPass::RecordInput& aInput ) {
    if ( !aInput.mySettings.myTemporalEnabled ) {
        if ( aInput.myTemporalHistoryReady != nullptr ) {
            *aInput.myTemporalHistoryReady = false;
        }
        return;
    }
    if ( aInput.myTemporalReadIndex == nullptr || aInput.myTemporalHistoryReady == nullptr ) {
        return;
    }

    if ( aInput.myPrepareTemporalDescriptors != nullptr ) {
        aInput.myPrepareTemporalDescriptors( aInput.myPrepareTemporalUser, aInput.myFrameIndex );
    }

    const uint32_t    readIndex    = ( *aInput.myTemporalReadIndex ) % 2u;
    const uint32_t    writeIndex   = ( readIndex + 1u ) % 2u;
    const Rhi_Texture historyRead  = readIndex == 0 ? aGpu.myAoHistory0 : aGpu.myAoHistory1;
    const Rhi_Texture historyWrite = writeIndex == 0 ? aGpu.myAoHistory0 : aGpu.myAoHistory1;

    BarrierColor( aCmd, aGpu.myAoRaw, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader );
    aInput.myLayouts->myAoRaw = Rhi_ImageLayout::ShaderReadOnly;

    const bool              historyReady        = *aInput.myTemporalHistoryReady;
    const Rhi_ImageLayout   historyReadOld      = historyReady ? Rhi_ImageLayout::ShaderReadOnly : Rhi_ImageLayout::Undefined;
    const Rhi_PipelineStage historyReadSrcStage = historyReady ? Rhi_PipelineStage::ComputeShader : Rhi_PipelineStage::TopOfPipe;
    BarrierColor( aCmd, historyRead, historyReadOld, Rhi_ImageLayout::ShaderReadOnly, historyReady ? Rhi_Access::ShaderWrite : Rhi_Access::None, Rhi_Access::ShaderRead,
                  historyReadSrcStage, Rhi_PipelineStage::ComputeShader );

    const Rhi_ImageLayout   historyWriteOld      = historyReady ? Rhi_ImageLayout::ShaderReadOnly : Rhi_ImageLayout::Undefined;
    const Rhi_PipelineStage historyWriteSrcStage = historyReady ? Rhi_PipelineStage::ComputeShader : Rhi_PipelineStage::TopOfPipe;
    BarrierColor( aCmd, historyWrite, historyWriteOld, Rhi_ImageLayout::General, Rhi_Access::None, Rhi_Access::ShaderWrite, historyWriteSrcStage,
                  Rhi_PipelineStage::ComputeShader );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myTemporalPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myTemporalLayout, 0, aGpu.myTemporalSet );

    AoTemporalPushConstants push{};
    push.historyBlend = std::clamp( aInput.mySettings.myTemporalBlend, 0.0f, 0.98f );
    push.historyValid = ( aInput.mySharedTemporalHistoryValid && historyReady ) ? 1.0f : 0.0f;
    Rhi::CommandListPushConstants( aCmd, aGpu.myTemporalLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );

    MaybeLabelBegin( aCmd, aInput.myDebugLabels, "Pass=AO Temporal" );
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    MaybeLabelEnd( aCmd, aInput.myDebugLabels );

    BarrierColor( aCmd, historyWrite, Rhi_ImageLayout::General, Rhi_ImageLayout::TransferSrc, Rhi_Access::ShaderWrite, Rhi_Access::TransferRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::Transfer );
    BarrierColor( aCmd, aGpu.myAoRaw, Rhi_ImageLayout::ShaderReadOnly, Rhi_ImageLayout::TransferDst, Rhi_Access::ShaderRead, Rhi_Access::TransferWrite,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::Transfer );

    Rhi::ImageCopy copy{};
    copy.mySrc       = historyWrite;
    copy.myDst       = aGpu.myAoRaw;
    copy.mySrcLayout = Rhi_ImageLayout::TransferSrc;
    copy.myDstLayout = Rhi_ImageLayout::TransferDst;
    copy.myWidth     = aInput.myWidth;
    copy.myHeight    = aInput.myHeight;
    Rhi::CommandListCopyImage( aCmd, copy );

    BarrierColor( aCmd, aGpu.myAoRaw, Rhi_ImageLayout::TransferDst, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::TransferWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::Transfer, Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader );
    BarrierColor( aCmd, historyWrite, Rhi_ImageLayout::TransferSrc, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::TransferRead, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::Transfer, Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader );

    *aInput.myTemporalReadIndex    = writeIndex;
    *aInput.myTemporalHistoryReady = true;
    aInput.myLayouts->myAoRaw      = Rhi_ImageLayout::ShaderReadOnly;
}

void TransitionRawForDeferred( Rhi_CommandList& aCmd, Rhi_Texture aRaw, Gfx_AoPass::LayoutState& aLayouts ) {
    BarrierColor( aCmd, aRaw, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead, Rhi_PipelineStage::ComputeShader,
                  Rhi_PipelineStage::FragmentShader );
    aLayouts.myAoRaw = Rhi_ImageLayout::ShaderReadOnly;
}

}  // namespace

namespace Gfx_AoPass {

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) || aInput.myLayouts == nullptr || aInput.myWidth == 0 || aInput.myHeight == 0 ) {
        return;
    }

    BarrierGBufferForAoRead( aCmd, aGpu );
    BarrierForComputeWrite( aCmd, aGpu.myAoRaw, aInput.myLayouts->myAoRaw );

    if ( aInput.mySettings.myMethod == Gfx_AoMethod::HbaoPlus ) {
        RecordHbaoPlus( aCmd, aGpu, aInput );
    }
    else if ( aInput.mySettings.myMethod == Gfx_AoMethod::Gtao ) {
        RecordGtao( aCmd, aGpu, aInput );
    }
    else {
        RecordClassicSsao( aCmd, aGpu, aInput );
    }

    RecordTemporalFilter( aCmd, aGpu, aInput );

    if ( aInput.myContactSoftActive ) {
        return;
    }

    if ( aInput.mySettings.myMethod == Gfx_AoMethod::ClassicSsao ) {
        RecordClassicBlur( aCmd, aGpu, aInput );
    }

    if ( !aInput.mySettings.myTemporalEnabled ) {
        TransitionRawForDeferred( aCmd, aGpu.myAoRaw, *aInput.myLayouts );
    }
}

}  // namespace Gfx_AoPass

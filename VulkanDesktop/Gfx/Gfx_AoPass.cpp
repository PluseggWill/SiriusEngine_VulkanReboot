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

bool CreateComputePipeline( Rhi_Device& aDevice, const void* aSpirv, size_t aSpirvBytes, Rhi_DescriptorSetLayout aSetLayout, size_t aPushBytes, Rhi_PipelineLayout& aOutLayout,
                            Rhi_Pipeline& aOutPipeline ) {
    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Compute;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = static_cast< uint32_t >( aPushBytes );

    Rhi::PipelineLayoutDesc layoutDesc{};
    layoutDesc.mySetLayouts     = &aSetLayout;
    layoutDesc.mySetLayoutCount = 1;
    layoutDesc.myPushRanges     = &push;
    layoutDesc.myPushRangeCount = 1;
    aOutLayout                  = Rhi::DeviceCreatePipelineLayout( aDevice, layoutDesc );
    if ( !aOutLayout ) {
        return false;
    }

    Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( aDevice, aSpirv, aSpirvBytes );
    if ( !shader ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aOutLayout );
        aOutLayout = {};
        return false;
    }
    Rhi::ComputePipelineDesc pipeDesc{};
    pipeDesc.myShader = shader;
    pipeDesc.myLayout = aOutLayout;
    aOutPipeline      = Rhi::DeviceCreateComputePipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, shader );
    if ( !aOutPipeline ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aOutLayout );
        aOutLayout = {};
        return false;
    }
    return true;
}

Rhi_Texture CreateAoStorageTexture( Rhi_Device& aDevice, uint32_t aWidth, uint32_t aHeight, Rhi_Format aFormat ) {
    Rhi::TextureDesc desc{};
    desc.myWidth  = aWidth;
    desc.myHeight = aHeight;
    desc.myFormat = aFormat;
    desc.myUsage  = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage;
    desc.myMemory = Rhi_MemoryUsage::GpuOnly;
    return Rhi::DeviceCreateTexture( aDevice, desc );
}

void DestroyAoImagesOnly( Rhi_Device& aDevice, Gfx_AoPass::PassState& aState ) {
    Rhi::DeviceDestroyTexture( aDevice, aState.myAoRaw );
    Rhi::DeviceDestroyTexture( aDevice, aState.myAoHalf );
    Rhi::DeviceDestroyTexture( aDevice, aState.myBentNormalHalf );
    Rhi::DeviceDestroyTexture( aDevice, aState.myAoBlur );
    Rhi::DeviceDestroyTexture( aDevice, aState.myAoHistory0 );
    Rhi::DeviceDestroyTexture( aDevice, aState.myAoHistory1 );
    aState.myWidth       = 0;
    aState.myHeight      = 0;
    aState.myImagesReady = false;
}

}  // namespace

namespace Gfx_AoPass {

bool CreatePipelines( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myClassicSpirvCode == nullptr || aDesc.myClassicSpirvBytes == 0 || aDesc.myHbaoSpirvCode == nullptr
         || aDesc.myHbaoSpirvBytes == 0 || aDesc.myGtaoSpirvCode == nullptr || aDesc.myGtaoSpirvBytes == 0 || aDesc.myUpsampleSpirvCode == nullptr
         || aDesc.myUpsampleSpirvBytes == 0 || aDesc.myBlurSpirvCode == nullptr || aDesc.myBlurSpirvBytes == 0 || aDesc.myTemporalSpirvCode == nullptr
         || aDesc.myTemporalSpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 4 > classicBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           classicLayoutDesc{};
    classicLayoutDesc.myBindings     = classicBindings.data();
    classicLayoutDesc.myBindingCount = static_cast< uint32_t >( classicBindings.size() );
    aState.myClassicSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, classicLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 5 > halfResBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 4, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           halfResLayoutDesc{};
    halfResLayoutDesc.myBindings     = halfResBindings.data();
    halfResLayoutDesc.myBindingCount = static_cast< uint32_t >( halfResBindings.size() );
    aState.myHalfResSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, halfResLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 3 > upsampleBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           upsampleLayoutDesc{};
    upsampleLayoutDesc.myBindings     = upsampleBindings.data();
    upsampleLayoutDesc.myBindingCount = static_cast< uint32_t >( upsampleBindings.size() );
    aState.myUpsampleSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, upsampleLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 2 > blurBindings = { {
        { 0, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           blurLayoutDesc{};
    blurLayoutDesc.myBindings     = blurBindings.data();
    blurLayoutDesc.myBindingCount = static_cast< uint32_t >( blurBindings.size() );
    aState.myBlurSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, blurLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 4 > temporalBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           temporalLayoutDesc{};
    temporalLayoutDesc.myBindings     = temporalBindings.data();
    temporalLayoutDesc.myBindingCount = static_cast< uint32_t >( temporalBindings.size() );
    aState.myTemporalSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, temporalLayoutDesc );

    if ( !aState.myClassicSetLayout || !aState.myHalfResSetLayout || !aState.myUpsampleSetLayout || !aState.myBlurSetLayout || !aState.myTemporalSetLayout ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    if ( !CreateComputePipeline( aDevice, aDesc.myClassicSpirvCode, aDesc.myClassicSpirvBytes, aState.myClassicSetLayout, sizeof( ClassicAoPushConstants ),
                                 aState.myClassicLayout, aState.myClassicPipeline ) ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc halfPush{};
    halfPush.myStages      = Rhi_ShaderStage::Compute;
    halfPush.myOffsetBytes = 0;
    halfPush.mySizeBytes   = static_cast< uint32_t >( sizeof( HalfResAoPushConstants ) );
    Rhi::PipelineLayoutDesc halfLayoutDesc{};
    halfLayoutDesc.mySetLayouts     = &aState.myHalfResSetLayout;
    halfLayoutDesc.mySetLayoutCount = 1;
    halfLayoutDesc.myPushRanges     = &halfPush;
    halfLayoutDesc.myPushRangeCount = 1;
    aState.myHalfResLayout          = Rhi::DeviceCreatePipelineLayout( aDevice, halfLayoutDesc );
    if ( !aState.myHalfResLayout ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    auto createWithLayout = [ & ]( const void* spirv, size_t bytes, Rhi_Pipeline& outPipe ) -> bool {
        Rhi_ShaderModule shader = Rhi::DeviceCreateShaderModule( aDevice, spirv, bytes );
        if ( !shader ) {
            return false;
        }
        Rhi::ComputePipelineDesc pipeDesc{};
        pipeDesc.myShader = shader;
        pipeDesc.myLayout = aState.myHalfResLayout;
        outPipe           = Rhi::DeviceCreateComputePipeline( aDevice, pipeDesc );
        Rhi::DeviceDestroyShaderModule( aDevice, shader );
        return static_cast< bool >( outPipe );
    };

    if ( !createWithLayout( aDesc.myHbaoSpirvCode, aDesc.myHbaoSpirvBytes, aState.myHbaoPipeline )
         || !createWithLayout( aDesc.myGtaoSpirvCode, aDesc.myGtaoSpirvBytes, aState.myGtaoPipeline ) ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    if ( !CreateComputePipeline( aDevice, aDesc.myUpsampleSpirvCode, aDesc.myUpsampleSpirvBytes, aState.myUpsampleSetLayout, sizeof( AoUpsamplePushConstants ),
                                 aState.myUpsampleLayout, aState.myUpsamplePipeline )
         || !CreateComputePipeline( aDevice, aDesc.myBlurSpirvCode, aDesc.myBlurSpirvBytes, aState.myBlurSetLayout, sizeof( AoBlurPushConstants ), aState.myBlurLayout,
                                    aState.myBlurPipeline )
         || !CreateComputePipeline( aDevice, aDesc.myTemporalSpirvCode, aDesc.myTemporalSpirvBytes, aState.myTemporalSetLayout, sizeof( AoTemporalPushConstants ),
                                    aState.myTemporalLayout, aState.myTemporalPipeline ) ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    Rhi::SamplerDesc samplerDesc{};
    samplerDesc.myMagFilter = Rhi_Filter::Linear;
    samplerDesc.myMinFilter = Rhi_Filter::Linear;
    samplerDesc.myAddressU  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressV  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressW  = Rhi_AddressMode::ClampToEdge;
    aState.myGBufferSampler = Rhi::DeviceCreateSampler( aDevice, samplerDesc );
    if ( !aState.myGBufferSampler ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    const uint32_t                                 frames    = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const std::array< Rhi::DescriptorPoolSize, 2 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 11u },
        { Rhi_DescriptorType::StorageImage, frames * 11u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames * 6u;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    aState.myPool            = Rhi::DeviceCreateDescriptorPool( aDevice, poolDesc );
    if ( !aState.myPool ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    if ( !aState.mySetsAllocated ) {
        for ( uint32_t i = 0; i < frames && i < kMaxFramesInFlight; ++i ) {
            aState.myClassicSets[ i ]   = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myClassicSetLayout );
            aState.myHalfResSets[ i ]   = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myHalfResSetLayout );
            aState.myUpsampleSets[ i ]  = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myUpsampleSetLayout );
            aState.myBlurHorizSets[ i ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myBlurSetLayout );
            aState.myBlurVertSets[ i ]  = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myBlurSetLayout );
            aState.myTemporalSets[ i ]  = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myTemporalSetLayout );
            if ( !aState.myClassicSets[ i ] || !aState.myHalfResSets[ i ] || !aState.myUpsampleSets[ i ] || !aState.myBlurHorizSets[ i ] || !aState.myBlurVertSets[ i ]
                 || !aState.myTemporalSets[ i ] ) {
                DestroyPipelines( aDevice, aState );
                return false;
            }
        }
        aState.mySetsAllocated = true;
    }

    aState.myPipelineReady = true;
    return true;
}

bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myPipelineReady || aDesc.myWidth == 0 || aDesc.myHeight == 0 ) {
        return false;
    }
    if ( aState.myImagesReady && aState.myWidth == aDesc.myWidth && aState.myHeight == aDesc.myHeight ) {
        return true;
    }

    DestroyAoImagesOnly( aDevice, aState );

    const uint32_t halfW = HalfDim( aDesc.myWidth );
    const uint32_t halfH = HalfDim( aDesc.myHeight );

    aState.myAoRaw          = CreateAoStorageTexture( aDevice, aDesc.myWidth, aDesc.myHeight, Rhi_Format::R8_Unorm );
    aState.myAoBlur         = CreateAoStorageTexture( aDevice, aDesc.myWidth, aDesc.myHeight, Rhi_Format::R8_Unorm );
    aState.myAoHistory0     = CreateAoStorageTexture( aDevice, aDesc.myWidth, aDesc.myHeight, Rhi_Format::R8_Unorm );
    aState.myAoHistory1     = CreateAoStorageTexture( aDevice, aDesc.myWidth, aDesc.myHeight, Rhi_Format::R8_Unorm );
    aState.myAoHalf         = CreateAoStorageTexture( aDevice, halfW, halfH, Rhi_Format::R8_Unorm );
    aState.myBentNormalHalf = CreateAoStorageTexture( aDevice, halfW, halfH, Rhi_Format::RG8_Unorm );

    if ( !aState.myAoRaw || !aState.myAoBlur || !aState.myAoHistory0 || !aState.myAoHistory1 || !aState.myAoHalf || !aState.myBentNormalHalf ) {
        DestroyAoImagesOnly( aDevice, aState );
        return false;
    }

    Rhi::DeviceTransitionTextureLayout( aDevice, aState.myBentNormalHalf, Rhi_ImageLayout::Undefined, Rhi_ImageLayout::ShaderReadOnly );

    aState.myWidth       = aDesc.myWidth;
    aState.myHeight      = aDesc.myHeight;
    aState.myImagesReady = true;
    return true;
}

void DestroyImages( Rhi_Device& aDevice, PassState& aState ) {
    DestroyAoImagesOnly( aDevice, aState );
}

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || !aState.myImagesReady ) {
        return;
    }
    if ( !aDesc.myGBufferDepth || !aDesc.myGBufferNormal || !aDesc.myGBufferWorldPos ) {
        return;
    }

    const uint32_t frames = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    for ( uint32_t i = 0; i < frames; ++i ) {
        const std::array< Rhi::DescriptorImageWrite, 4 > classic = { {
            { aState.myClassicSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferDepth, Rhi_ImageLayout::DepthStencilReadOnly },
            { aState.myClassicSets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferNormal, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myClassicSets[ i ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferWorldPos, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myClassicSets[ i ], 3, Rhi_DescriptorType::StorageImage, {}, aState.myAoRaw, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, classic.data(), static_cast< uint32_t >( classic.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 5 > halfRes = { {
            { aState.myHalfResSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferDepth, Rhi_ImageLayout::DepthStencilReadOnly },
            { aState.myHalfResSets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferNormal, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myHalfResSets[ i ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferWorldPos, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myHalfResSets[ i ], 3, Rhi_DescriptorType::StorageImage, {}, aState.myAoHalf, Rhi_ImageLayout::General },
            { aState.myHalfResSets[ i ], 4, Rhi_DescriptorType::StorageImage, {}, aState.myBentNormalHalf, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, halfRes.data(), static_cast< uint32_t >( halfRes.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 3 > upsample = { {
            { aState.myUpsampleSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aState.myAoHalf, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myUpsampleSets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferDepth, Rhi_ImageLayout::DepthStencilReadOnly },
            { aState.myUpsampleSets[ i ], 2, Rhi_DescriptorType::StorageImage, {}, aState.myAoRaw, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, upsample.data(), static_cast< uint32_t >( upsample.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 2 > blurH = { {
            { aState.myBlurHorizSets[ i ], 0, Rhi_DescriptorType::StorageImage, {}, aState.myAoRaw, Rhi_ImageLayout::General },
            { aState.myBlurHorizSets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myAoBlur, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, blurH.data(), static_cast< uint32_t >( blurH.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 2 > blurV = { {
            { aState.myBlurVertSets[ i ], 0, Rhi_DescriptorType::StorageImage, {}, aState.myAoBlur, Rhi_ImageLayout::General },
            { aState.myBlurVertSets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myAoRaw, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, blurV.data(), static_cast< uint32_t >( blurV.size() ) );

        UpdateTemporalDescriptors( aDevice, aDesc, aState, i );
    }
}

void UpdateTemporalDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState, uint32_t aFrameIndex ) {
    if ( !aState.mySetsAllocated || !aState.myImagesReady || !aDesc.myMotionVector || aFrameIndex >= kMaxFramesInFlight ) {
        return;
    }

    const uint32_t    readIndex    = aDesc.myTemporalReadIndex % 2u;
    const uint32_t    writeIndex   = ( readIndex + 1u ) % 2u;
    const Rhi_Texture historyRead  = ( readIndex == 0 ) ? aState.myAoHistory0 : aState.myAoHistory1;
    const Rhi_Texture historyWrite = ( writeIndex == 0 ) ? aState.myAoHistory0 : aState.myAoHistory1;

    const std::array< Rhi::DescriptorImageWrite, 4 > temporal = { {
        { aState.myTemporalSets[ aFrameIndex ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myMotionVector, Rhi_ImageLayout::ShaderReadOnly },
        { aState.myTemporalSets[ aFrameIndex ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aState.myAoRaw, Rhi_ImageLayout::ShaderReadOnly },
        { aState.myTemporalSets[ aFrameIndex ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, historyRead, Rhi_ImageLayout::ShaderReadOnly },
        { aState.myTemporalSets[ aFrameIndex ], 3, Rhi_DescriptorType::StorageImage, {}, historyWrite, Rhi_ImageLayout::General },
    } };
    Rhi::DeviceUpdateDescriptorImages( aDevice, temporal.data(), static_cast< uint32_t >( temporal.size() ) );
}

void DestroyPipelines( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImages( aDevice, aState );

    for ( auto& set : aState.myClassicSets ) {
        set = {};
    }
    for ( auto& set : aState.myHalfResSets ) {
        set = {};
    }
    for ( auto& set : aState.myUpsampleSets ) {
        set = {};
    }
    for ( auto& set : aState.myBlurHorizSets ) {
        set = {};
    }
    for ( auto& set : aState.myBlurVertSets ) {
        set = {};
    }
    for ( auto& set : aState.myTemporalSets ) {
        set = {};
    }
    aState.mySetsAllocated = false;

    if ( aState.myClassicPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myClassicPipeline );
    }
    if ( aState.myHbaoPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myHbaoPipeline );
    }
    if ( aState.myGtaoPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myGtaoPipeline );
    }
    if ( aState.myUpsamplePipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myUpsamplePipeline );
    }
    if ( aState.myBlurPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myBlurPipeline );
    }
    if ( aState.myTemporalPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myTemporalPipeline );
    }
    if ( aState.myClassicLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myClassicLayout );
    }
    if ( aState.myHalfResLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myHalfResLayout );
    }
    if ( aState.myUpsampleLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myUpsampleLayout );
    }
    if ( aState.myBlurLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myBlurLayout );
    }
    if ( aState.myTemporalLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myTemporalLayout );
    }
    if ( aState.myPool ) {
        Rhi::DeviceDestroyDescriptorPool( aDevice, aState.myPool );
    }
    if ( aState.myClassicSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myClassicSetLayout );
    }
    if ( aState.myHalfResSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myHalfResSetLayout );
    }
    if ( aState.myUpsampleSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myUpsampleSetLayout );
    }
    if ( aState.myBlurSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myBlurSetLayout );
    }
    if ( aState.myTemporalSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myTemporalSetLayout );
    }
    if ( aState.myGBufferSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myGBufferSampler );
    }
    aState.myPipelineReady = false;
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyPipelines( aDevice, aState );
}

}  // namespace Gfx_AoPass

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

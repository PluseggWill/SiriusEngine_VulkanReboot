#include "Gfx_SsrPass.h"

#include <array>

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

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.mySpirvCode == nullptr || aDesc.mySpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 7 > bindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 4, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 5, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 6, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           setLayoutDesc{};
    setLayoutDesc.myBindings     = bindings.data();
    setLayoutDesc.myBindingCount = static_cast< uint32_t >( bindings.size() );
    aState.mySetLayout           = Rhi::DeviceCreateDescriptorSetLayout( aDevice, setLayoutDesc );
    if ( !aState.mySetLayout ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Compute;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = static_cast< uint32_t >( sizeof( TracePush ) );

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

    Rhi::SamplerDesc samplerDesc{};
    samplerDesc.myMagFilter = Rhi_Filter::Linear;
    samplerDesc.myMinFilter = Rhi_Filter::Linear;
    samplerDesc.myAddressU  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressV  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressW  = Rhi_AddressMode::ClampToEdge;
    aState.myGBufferSampler = Rhi::DeviceCreateSampler( aDevice, samplerDesc );
    if ( !aState.myGBufferSampler ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    const uint32_t                                 frames    = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const std::array< Rhi::DescriptorPoolSize, 2 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 7u },
        { Rhi_DescriptorType::StorageImage, frames },
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

    for ( uint32_t i = 0; i < frames; ++i ) {
        aState.mySets[ i ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.mySetLayout );
        if ( !aState.mySets[ i ] ) {
            DestroyPipeline( aDevice, aState );
            return false;
        }
    }
    aState.mySetsAllocated = true;

    aState.myPipelineReady = true;
    return true;
}

void DestroyPipeline( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImages( aDevice, aState );

    for ( auto& set : aState.mySets ) {
        set = {};
    }
    aState.mySetsAllocated = false;

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
    if ( aState.myGBufferSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myGBufferSampler );
    }
    aState.myPipelineReady = false;
}

namespace {

    void DestroySsrImagesOnly( Rhi_Device& aDevice, Gfx_SsrPass::PassState& aState ) {
        Rhi::DeviceDestroyTexture( aDevice, aState.mySsrOutput );
        Rhi::DeviceDestroyTexture( aDevice, aState.myHistory0 );
        Rhi::DeviceDestroyTexture( aDevice, aState.myHistory1 );
        aState.myWidth          = 0;
        aState.myHeight         = 0;
        aState.myImagesReady    = false;
        aState.mySsrLayout      = Rhi_ImageLayout::Undefined;
        aState.myHistoryLayouts = { Rhi_ImageLayout::Undefined, Rhi_ImageLayout::Undefined };
    }

}  // namespace

bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myPipelineReady || aDesc.myWidth == 0 || aDesc.myHeight == 0 ) {
        return false;
    }
    if ( aState.myImagesReady && aState.myWidth == aDesc.myWidth && aState.myHeight == aDesc.myHeight ) {
        return true;
    }

    DestroySsrImagesOnly( aDevice, aState );

    Rhi::TextureDesc outDesc{};
    outDesc.myWidth    = aDesc.myWidth;
    outDesc.myHeight   = aDesc.myHeight;
    outDesc.myFormat   = Rhi_Format::RGBA16_Sfloat;
    outDesc.myUsage    = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage;
    outDesc.myMemory   = Rhi_MemoryUsage::GpuOnly;
    aState.mySsrOutput = Rhi::DeviceCreateTexture( aDevice, outDesc );

    Rhi::TextureDesc histDesc{};
    histDesc.myWidth  = aDesc.myWidth;
    histDesc.myHeight = aDesc.myHeight;
    histDesc.myFormat = Rhi_Format::RGBA16_Sfloat;
    histDesc.myUsage  = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::TransferDst;
    histDesc.myMemory = Rhi_MemoryUsage::GpuOnly;
    aState.myHistory0 = Rhi::DeviceCreateTexture( aDevice, histDesc );
    aState.myHistory1 = Rhi::DeviceCreateTexture( aDevice, histDesc );

    if ( !aState.mySsrOutput || !aState.myHistory0 || !aState.myHistory1 ) {
        DestroySsrImagesOnly( aDevice, aState );
        return false;
    }

    Rhi::DeviceTransitionTextureLayout( aDevice, aState.myHistory0, Rhi_ImageLayout::Undefined, Rhi_ImageLayout::ShaderReadOnly );
    Rhi::DeviceTransitionTextureLayout( aDevice, aState.myHistory1, Rhi_ImageLayout::Undefined, Rhi_ImageLayout::ShaderReadOnly );
    aState.myHistoryLayouts[ 0 ] = Rhi_ImageLayout::ShaderReadOnly;
    aState.myHistoryLayouts[ 1 ] = Rhi_ImageLayout::ShaderReadOnly;
    aState.myWidth               = aDesc.myWidth;
    aState.myHeight              = aDesc.myHeight;
    aState.myImagesReady         = true;
    return true;
}

void DestroyImages( Rhi_Device& aDevice, PassState& aState ) {
    DestroySsrImagesOnly( aDevice, aState );
}

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || !aState.myImagesReady ) {
        return;
    }
    if ( !aDesc.myGBufferDepth || !aDesc.myGBufferNormal || !aDesc.myGBufferWorldPos || !aDesc.myGBufferAlbedo || !aDesc.myHistoryRead || !aDesc.myHiZ
         || !aDesc.myHiZSampler ) {
        return;
    }

    const uint32_t frames = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const uint32_t begin  = ( aDesc.myFrameIndex == 0xffffffffu ) ? 0u : aDesc.myFrameIndex;
    const uint32_t end    = ( aDesc.myFrameIndex == 0xffffffffu ) ? frames : ( aDesc.myFrameIndex + 1u );
    if ( begin >= frames || begin >= end ) {
        return;
    }
    for ( uint32_t i = begin; i < end && i < frames; ++i ) {
        const std::array< Rhi::DescriptorImageWrite, 7 > writes = { {
            { aState.mySets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferDepth, Rhi_ImageLayout::DepthStencilReadOnly },
            { aState.mySets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferNormal, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferWorldPos, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 3, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myHistoryRead, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 4, Rhi_DescriptorType::CombinedImageSampler, aDesc.myHiZSampler, aDesc.myHiZ, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 5, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myGBufferAlbedo, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 6, Rhi_DescriptorType::StorageImage, {}, aState.mySsrOutput, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, writes.data(), static_cast< uint32_t >( writes.size() ) );
    }
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyPipeline( aDevice, aState );
}

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

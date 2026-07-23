#include "Gfx_DepthPyramidPass.h"

#include "Gfx_AoSettings.h"

#include <algorithm>
#include <array>

namespace {

struct DepthPyramidPushConstants {
    uint32_t mipLevel;
    uint32_t isFirstMip;
    uint32_t srcWidth;
    uint32_t srcHeight;
};

static_assert( sizeof( DepthPyramidPushConstants ) == 16, "DepthPyramidPushConstants must match DepthPyramid.comp push block" );

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

void BarrierPyramid( Rhi_CommandList& aCmd, Rhi_Texture aPyramid, Rhi_ImageLayout aOldLayout, Rhi_ImageLayout aNewLayout, Rhi_Access aSrcAccess, Rhi_Access aDstAccess,
                     Rhi_PipelineStage aSrcStage, Rhi_PipelineStage aDstStage, uint32_t aBaseMip, uint32_t aMipCount ) {
    Rhi::ImageBarrier barrier{};
    barrier.myTexture   = aPyramid;
    barrier.myOldLayout = aOldLayout;
    barrier.myNewLayout = aNewLayout;
    barrier.mySrcAccess = aSrcAccess;
    barrier.myDstAccess = aDstAccess;
    barrier.mySrcStage  = aSrcStage;
    barrier.myDstStage  = aDstStage;
    barrier.myBaseMip   = aBaseMip;
    barrier.myMipCount  = aMipCount;
    Rhi::CommandListPipelineBarrier( aCmd, &barrier, 1 );
}

void BarrierForComputeWrite( Rhi_CommandList& aCmd, Rhi_Texture aPyramid, uint32_t aMipCount, Rhi_ImageLayout& aInOutLayout ) {
    const Rhi_ImageLayout   oldLayout = aInOutLayout;
    const Rhi_Access        srcAccess = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_Access::None : Rhi_Access::ShaderRead;
    const Rhi_PipelineStage srcStage  = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_PipelineStage::TopOfPipe : Rhi_PipelineStage::ComputeShader;
    BarrierPyramid( aCmd, aPyramid, oldLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader, 0, aMipCount );
    aInOutLayout = Rhi_ImageLayout::General;
}

void FillDescriptorSets( Rhi_Device& aDevice, Gfx_DepthPyramidPass::PassState& aState, Rhi_Texture aGBufferDepth, uint32_t aFramesInFlight ) {
    using namespace Gfx_DepthPyramidPass;
    const uint32_t frames = aFramesInFlight > 0u ? aFramesInFlight : kMaxFramesInFlight;
    for ( uint32_t frame = 0; frame < frames && frame < kMaxFramesInFlight; ++frame ) {
        for ( uint32_t mip = 0; mip < aState.myMipLevelCount; ++mip ) {
            const uint32_t                             srcMip = mip > 0u ? mip - 1u : 0u;
            std::array< Rhi::DescriptorImageWrite, 3 > writes{};
            writes[ 0 ].mySet     = aState.mySets[ frame ][ mip ];
            writes[ 0 ].myBinding = 0;
            writes[ 0 ].myType    = Rhi_DescriptorType::CombinedImageSampler;
            writes[ 0 ].mySampler = aState.myDepthSampler;
            writes[ 0 ].myTexture = aGBufferDepth;
            writes[ 0 ].myLayout  = Rhi_ImageLayout::DepthStencilReadOnly;

            writes[ 1 ].mySet     = aState.mySets[ frame ][ mip ];
            writes[ 1 ].myBinding = 1;
            writes[ 1 ].myType    = Rhi_DescriptorType::StorageImage;
            writes[ 1 ].myTexture = aState.myMipViews[ srcMip ];
            writes[ 1 ].myLayout  = Rhi_ImageLayout::General;

            writes[ 2 ].mySet     = aState.mySets[ frame ][ mip ];
            writes[ 2 ].myBinding = 2;
            writes[ 2 ].myType    = Rhi_DescriptorType::StorageImage;
            writes[ 2 ].myTexture = aState.myMipViews[ mip ];
            writes[ 2 ].myLayout  = Rhi_ImageLayout::General;

            Rhi::DeviceUpdateDescriptorImages( aDevice, writes.data(), static_cast< uint32_t >( writes.size() ) );
        }
    }
}

void DestroyPyramidOnly( Rhi_Device& aDevice, Gfx_DepthPyramidPass::PassState& aState ) {
    for ( auto& view : aState.myMipViews ) {
        if ( view ) {
            Rhi::DeviceDestroyTexture( aDevice, view );
        }
    }
    if ( aState.myPyramid ) {
        Rhi::DeviceDestroyTexture( aDevice, aState.myPyramid );
    }
    aState.myMipLevelCount = 0;
    aState.myWidth         = 0;
    aState.myHeight        = 0;
    aState.myPyramidLayout = Rhi_ImageLayout::Undefined;
    aState.myInitialized   = false;
}

}  // namespace

namespace Gfx_DepthPyramidPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.mySpirvCode == nullptr || aDesc.mySpirvBytes == 0 ) {
        return false;
    }

    Rhi::SamplerDesc depthSamplerDesc{};
    depthSamplerDesc.myMagFilter = Rhi_Filter::Nearest;
    depthSamplerDesc.myMinFilter = Rhi_Filter::Nearest;
    aState.myDepthSampler        = Rhi::DeviceCreateSampler( aDevice, depthSamplerDesc );

    Rhi::SamplerDesc pyramidSamplerDesc{};
    pyramidSamplerDesc.myMagFilter  = Rhi_Filter::Nearest;
    pyramidSamplerDesc.myMinFilter  = Rhi_Filter::Nearest;
    pyramidSamplerDesc.myMipmapMode = Rhi_MipmapMode::Nearest;
    pyramidSamplerDesc.myMaxLod     = static_cast< float >( kMaxMipLevels );
    aState.myPyramidSampler         = Rhi::DeviceCreateSampler( aDevice, pyramidSamplerDesc );
    if ( !aState.myDepthSampler || !aState.myPyramidSampler ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 3 > bindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
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
    const std::array< Rhi::DescriptorPoolSize, 2 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * kMaxMipLevels },
        { Rhi_DescriptorType::StorageImage, frames * kMaxMipLevels * 2u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames * kMaxMipLevels;
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
    push.mySizeBytes   = sizeof( DepthPyramidPushConstants );

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

    aState.myPipelineReady = true;
    return true;
}

bool CreateOrRecreateImage( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myPipelineReady || !aDesc.myGBufferDepth || aDesc.myWidth == 0 || aDesc.myHeight == 0 ) {
        return false;
    }

    const uint32_t mipCount = std::min( Gfx_ComputeHiZMipLevelCount( aDesc.myWidth, aDesc.myHeight, kMaxMipLevels ), kMaxMipLevels );
    const uint32_t frames   = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;

    if ( aState.myPyramid && aState.myWidth == aDesc.myWidth && aState.myHeight == aDesc.myHeight && aState.myMipLevelCount == mipCount ) {
        FillDescriptorSets( aDevice, aState, aDesc.myGBufferDepth, frames );
        aState.myInitialized = true;
        return true;
    }

    DestroyPyramidOnly( aDevice, aState );

    Rhi::TextureDesc texDesc{};
    texDesc.myWidth     = aDesc.myWidth;
    texDesc.myHeight    = aDesc.myHeight;
    texDesc.myMipLevels = mipCount;
    texDesc.myFormat    = Rhi_Format::R32_Sfloat;
    texDesc.myUsage     = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage;
    texDesc.myMemory    = Rhi_MemoryUsage::GpuOnly;
    aState.myPyramid    = Rhi::DeviceCreateTexture( aDevice, texDesc );
    if ( !aState.myPyramid ) {
        return false;
    }

    aState.myMipLevelCount = mipCount;
    aState.myWidth         = aDesc.myWidth;
    aState.myHeight        = aDesc.myHeight;
    aState.myPyramidLayout = Rhi_ImageLayout::Undefined;

    for ( uint32_t mip = 0; mip < mipCount; ++mip ) {
        aState.myMipViews[ mip ] = Rhi::DeviceCreateTextureMipView( aDevice, aState.myPyramid, mip );
        if ( !aState.myMipViews[ mip ] ) {
            DestroyPyramidOnly( aDevice, aState );
            return false;
        }
    }

    if ( !aState.mySetsAllocated ) {
        for ( uint32_t frame = 0; frame < frames && frame < kMaxFramesInFlight; ++frame ) {
            for ( uint32_t mip = 0; mip < kMaxMipLevels; ++mip ) {
                aState.mySets[ frame ][ mip ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.mySetLayout );
                if ( !aState.mySets[ frame ][ mip ] ) {
                    DestroyPyramidOnly( aDevice, aState );
                    return false;
                }
            }
        }
        aState.mySetsAllocated = true;
    }

    FillDescriptorSets( aDevice, aState, aDesc.myGBufferDepth, frames );
    aState.myInitialized = true;
    return true;
}

void DestroyImage( Rhi_Device& aDevice, PassState& aState ) {
    DestroyPyramidOnly( aDevice, aState );
}

void DestroyPipeline( Rhi_Device& aDevice, PassState& aState ) {
    for ( auto& frameSets : aState.mySets ) {
        for ( auto& set : frameSets ) {
            set = {};
        }
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
    if ( aState.myPyramidSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myPyramidSampler );
    }
    if ( aState.myDepthSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myDepthSampler );
    }
    aState.myPipelineReady = false;
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImage( aDevice, aState );
    DestroyPipeline( aDevice, aState );
}

GpuResources MakeGpuResources( const PassState& aState, uint32_t aFrameIndex ) {
    GpuResources out{};
    if ( !aState.myInitialized || !aState.myPipelineReady ) {
        return out;
    }
    const uint32_t frame = aFrameIndex % kMaxFramesInFlight;
    out.myPipeline       = aState.myPipeline;
    out.myLayout         = aState.myLayout;
    out.myPyramid        = aState.myPyramid;
    out.myMipLevelCount  = aState.myMipLevelCount;
    for ( uint32_t mip = 0; mip < aState.myMipLevelCount && mip < kMaxMipLevels; ++mip ) {
        out.myMipSets[ mip ] = aState.mySets[ frame ][ mip ];
    }
    return out;
}

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput ) {
    if ( aGpu.myMipLevelCount == 0 || aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.myPyramidLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    BarrierForComputeWrite( aCmd, aGpu.myPyramid, aGpu.myMipLevelCount, *aInput.myPyramidLayout );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPipeline );

    for ( uint32_t dstMip = 0; dstMip < aGpu.myMipLevelCount; ++dstMip ) {
        const bool     isFirstMip = ( dstMip == 0 );
        const uint32_t srcMip     = dstMip > 0 ? dstMip - 1 : 0;
        const uint32_t srcWidth   = isFirstMip ? aInput.myWidth : std::max( 1u, aInput.myWidth >> srcMip );
        const uint32_t srcHeight  = isFirstMip ? aInput.myHeight : std::max( 1u, aInput.myHeight >> srcMip );
        const uint32_t dstWidth   = isFirstMip ? aInput.myWidth : std::max( 1u, aInput.myWidth >> dstMip );
        const uint32_t dstHeight  = isFirstMip ? aInput.myHeight : std::max( 1u, aInput.myHeight >> dstMip );

        Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myLayout, 0, aGpu.myMipSets[ dstMip ] );

        DepthPyramidPushConstants push{};
        push.mipLevel   = dstMip;
        push.isFirstMip = isFirstMip ? 1u : 0u;
        push.srcWidth   = srcWidth;
        push.srcHeight  = srcHeight;
        Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );

        if ( dstMip > 0 ) {
            BarrierPyramid( aCmd, aGpu.myPyramid, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                            Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader, srcMip, 1 );
        }

        MaybeLabelBegin( aCmd, aInput.myDebugLabels, "Pass=DepthPyramidMip" );
        Rhi::CommandListDispatch( aCmd, ( dstWidth + 7 ) / 8, ( dstHeight + 7 ) / 8, 1 );
        MaybeLabelEnd( aCmd, aInput.myDebugLabels );

        BarrierPyramid( aCmd, aGpu.myPyramid, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead | Rhi_Access::ShaderWrite,
                        Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader, dstMip, 1 );
    }

    BarrierPyramid( aCmd, aGpu.myPyramid, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                    Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader | Rhi_PipelineStage::ComputeShader, 0, aGpu.myMipLevelCount );
    *aInput.myPyramidLayout = Rhi_ImageLayout::ShaderReadOnly;
}

}  // namespace Gfx_DepthPyramidPass

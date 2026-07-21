#include "Gfx_DepthPyramidPass.h"

#include <algorithm>

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

}  // namespace

namespace Gfx_DepthPyramidPass {

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
        const uint32_t srcWidth   = isFirstMip ? aInput.myWidth : std::max( 1u, aInput.myWidth >> dstMip );
        const uint32_t srcHeight  = isFirstMip ? aInput.myHeight : std::max( 1u, aInput.myHeight >> dstMip );
        const uint32_t dstWidth   = std::max( 1u, srcWidth >> 1 );
        const uint32_t dstHeight  = std::max( 1u, srcHeight >> 1 );

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
                    Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader, 0, aGpu.myMipLevelCount );
    *aInput.myPyramidLayout = Rhi_ImageLayout::ShaderReadOnly;
}

}  // namespace Gfx_DepthPyramidPass

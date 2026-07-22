#include "Gfx_ShadowAoSoftPass.h"

#include <glm/glm.hpp>

namespace {

struct ShadowAoPackPushConstants {
    alignas( 16 ) glm::vec4 params;
};
static_assert( sizeof( ShadowAoPackPushConstants ) == 16, "ShadowAoPackPushConstants must match ShadowAoPack.comp" );

struct ShadowAoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
    float    radius;
    float    depthSigma;
};
static_assert( sizeof( ShadowAoBlurPushConstants ) == 16, "ShadowAoBlurPushConstants must match ShadowAoBlur.comp" );

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

void BarrierForComputeWrite( Rhi_CommandList& aCmd, Rhi_Texture aTex, Rhi_ImageLayout& aLayout ) {
    const Rhi_ImageLayout   oldLayout = aLayout;
    const Rhi_Access        srcAccess = oldLayout == Rhi_ImageLayout::Undefined ? Rhi_Access::None : Rhi_Access::ShaderRead;
    const Rhi_PipelineStage srcStage  = oldLayout == Rhi_ImageLayout::Undefined        ? Rhi_PipelineStage::TopOfPipe
                                        : oldLayout == Rhi_ImageLayout::ShaderReadOnly ? Rhi_PipelineStage::FragmentShader
                                                                                       : Rhi_PipelineStage::ComputeShader;
    BarrierColor( aCmd, aTex, oldLayout, Rhi_ImageLayout::General, srcAccess, Rhi_Access::ShaderWrite, srcStage, Rhi_PipelineStage::ComputeShader );
    aLayout = Rhi_ImageLayout::General;
}

void DispatchBlur( Rhi_CommandList& aCmd, const Gfx_ShadowAoSoftPass::GpuResources& aGpu, Rhi_DescriptorSet aSet, float aRadius, float aDepthSigma, uint32_t aAxisX,
                   uint32_t aAxisY, uint32_t aWidth, uint32_t aHeight, bool aDebug, const char* aLabel ) {
    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myBlurLayout, 0, aSet );
    ShadowAoBlurPushConstants push{};
    push.axisX      = aAxisX;
    push.axisY      = aAxisY;
    push.radius     = aRadius;
    push.depthSigma = aDepthSigma;
    Rhi::CommandListPushConstants( aCmd, aGpu.myBlurLayout, Rhi_ShaderStage::Compute, 0, sizeof( push ), &push );
    if ( aDebug ) {
        Rhi::CommandListBeginDebugLabel( aCmd, aLabel );
    }
    Rhi::CommandListDispatch( aCmd, ( aWidth + 7 ) / 8, ( aHeight + 7 ) / 8, 1 );
    if ( aDebug ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

}  // namespace

namespace Gfx_ShadowAoSoftPass {

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, RecordInput& aInput ) {
    if ( aInput.myWidth == 0 || aInput.myHeight == 0 || aInput.myPingLayout == nullptr || aInput.myPongLayout == nullptr ) {
        return;
    }
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    BarrierColor( aCmd, aGpu.myGBufferWorldPos, Rhi_ImageLayout::ShaderReadOnly, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ColorAttachmentRW, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ColorAttachment, Rhi_PipelineStage::ComputeShader );

    if ( aInput.myUseAoRaw && aInput.myAoRawLayout != nullptr && aGpu.myAoRaw.myId != 0 ) {
        if ( *aInput.myAoRawLayout != Rhi_ImageLayout::ShaderReadOnly ) {
            BarrierColor( aCmd, aGpu.myAoRaw, *aInput.myAoRawLayout, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderRead | Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                          Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::ComputeShader );
            *aInput.myAoRawLayout = Rhi_ImageLayout::ShaderReadOnly;
        }
    }

    BarrierForComputeWrite( aCmd, aGpu.mySoftPing, *aInput.myPingLayout );
    BarrierForComputeWrite( aCmd, aGpu.mySoftPong, *aInput.myPongLayout );

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPackPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Compute, aGpu.myPackLayout, 0, aGpu.myPackSet );
    ShadowAoPackPushConstants packPush{};
    packPush.params.x = aInput.myUseAoRaw ? 1.0f : 0.0f;
    Rhi::CommandListPushConstants( aCmd, aGpu.myPackLayout, Rhi_ShaderStage::Compute, 0, sizeof( packPush ), &packPush );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=ShadowAoPack" );
    }
    Rhi::CommandListDispatch( aCmd, ( aInput.myWidth + 7 ) / 8, ( aInput.myHeight + 7 ) / 8, 1 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }

    BarrierColor( aCmd, aGpu.mySoftPing, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead, Rhi_PipelineStage::ComputeShader,
                  Rhi_PipelineStage::ComputeShader );

    DispatchBlur( aCmd, aGpu, aGpu.myBlurHorizSet, aInput.myBlurRadius, aInput.myDepthSigma, 1, 0, aInput.myWidth, aInput.myHeight, aInput.myDebugLabels,
                  "Pass=ShadowAoBlurH" );

    BarrierColor( aCmd, aGpu.mySoftPong, Rhi_ImageLayout::General, Rhi_ImageLayout::General, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead, Rhi_PipelineStage::ComputeShader,
                  Rhi_PipelineStage::ComputeShader );

    DispatchBlur( aCmd, aGpu, aGpu.myBlurVertSet, aInput.myBlurRadius, aInput.myDepthSigma, 0, 1, aInput.myWidth, aInput.myHeight, aInput.myDebugLabels,
                  "Pass=ShadowAoBlurV" );

    BarrierColor( aCmd, aGpu.mySoftPing, Rhi_ImageLayout::General, Rhi_ImageLayout::ShaderReadOnly, Rhi_Access::ShaderWrite, Rhi_Access::ShaderRead,
                  Rhi_PipelineStage::ComputeShader, Rhi_PipelineStage::FragmentShader );
    *aInput.myPingLayout = Rhi_ImageLayout::ShaderReadOnly;
}

}  // namespace Gfx_ShadowAoSoftPass

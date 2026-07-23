#include "Gfx_ShadowAoSoftPass.h"

#include <glm/glm.hpp>

#include <array>

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

}  // namespace

namespace Gfx_ShadowAoSoftPass {

bool CreatePipelines( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myPackSpirvCode == nullptr || aDesc.myPackSpirvBytes == 0 || aDesc.myBlurSpirvCode == nullptr
         || aDesc.myBlurSpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 6 > packBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 4, Rhi_DescriptorType::UniformBuffer, 1, Rhi_ShaderStage::Compute },
        { 5, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           packSetLayoutDesc{};
    packSetLayoutDesc.myBindings     = packBindings.data();
    packSetLayoutDesc.myBindingCount = static_cast< uint32_t >( packBindings.size() );
    aState.myPackSetLayout           = Rhi::DeviceCreateDescriptorSetLayout( aDevice, packSetLayoutDesc );
    if ( !aState.myPackSetLayout ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 3 > blurBindings = { {
        { 0, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           blurSetLayoutDesc{};
    blurSetLayoutDesc.myBindings     = blurBindings.data();
    blurSetLayoutDesc.myBindingCount = static_cast< uint32_t >( blurBindings.size() );
    aState.myBlurSetLayout           = Rhi::DeviceCreateDescriptorSetLayout( aDevice, blurSetLayoutDesc );
    if ( !aState.myBlurSetLayout ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    if ( !CreateComputePipeline( aDevice, aDesc.myPackSpirvCode, aDesc.myPackSpirvBytes, aState.myPackSetLayout, sizeof( ShadowAoPackPushConstants ), aState.myPackLayout,
                                 aState.myPackPipeline ) ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }
    if ( !CreateComputePipeline( aDevice, aDesc.myBlurSpirvCode, aDesc.myBlurSpirvBytes, aState.myBlurSetLayout, sizeof( ShadowAoBlurPushConstants ), aState.myBlurLayout,
                                 aState.myBlurPipeline ) ) {
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
    const std::array< Rhi::DescriptorPoolSize, 3 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 10u },
        { Rhi_DescriptorType::StorageImage, frames * 10u },
        { Rhi_DescriptorType::UniformBuffer, frames * 2u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames * 5u;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    aState.myPool            = Rhi::DeviceCreateDescriptorPool( aDevice, poolDesc );
    if ( !aState.myPool ) {
        DestroyPipelines( aDevice, aState );
        return false;
    }

    aState.myPipelineReady = true;
    return true;
}

void DestroyPipelines( Rhi_Device& aDevice, PassState& aState ) {
    if ( aState.myPackPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myPackPipeline );
    }
    if ( aState.myBlurPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myBlurPipeline );
    }
    if ( aState.myPackLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myPackLayout );
    }
    if ( aState.myBlurLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myBlurLayout );
    }
    if ( aState.myPool ) {
        Rhi::DeviceDestroyDescriptorPool( aDevice, aState.myPool );
    }
    if ( aState.myPackSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myPackSetLayout );
    }
    if ( aState.myBlurSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myBlurSetLayout );
    }
    if ( aState.myGBufferSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.myGBufferSampler );
    }
    aState.myPipelineReady = false;
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyPipelines( aDevice, aState );
}

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

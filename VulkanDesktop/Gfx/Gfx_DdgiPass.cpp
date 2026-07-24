#include "Gfx_DdgiPass.h"

#include <array>

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

void DestroyDdgiImagesOnly( Rhi_Device& aDevice, Gfx_DdgiPass::PassState& aState ) {
    Rhi::DeviceDestroyTexture( aDevice, aState.myIrradianceAtlas );
    Rhi::DeviceDestroyTexture( aDevice, aState.myVisibilityAtlas );
    aState.myAtlasWidth  = 0;
    aState.myAtlasHeight = 0;
    aState.myImagesReady = false;
}

}  // namespace

namespace Gfx_DdgiPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.mySpirvCode == nullptr || aDesc.mySpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 4 > bindings = { {
        { 0, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
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
    push.mySizeBytes   = static_cast< uint32_t >( sizeof( ProbePush ) );

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
        { Rhi_DescriptorType::StorageImage, frames * 2u },
        { Rhi_DescriptorType::CombinedImageSampler, frames * 2u },
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

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImages( aDevice, aState );
    DestroyPipeline( aDevice, aState );
}

bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myPipelineReady || aDesc.myProbeCountX == 0 || aDesc.myProbeCountY == 0 || aDesc.myProbeCountZ == 0 ) {
        return false;
    }
    const uint32_t width  = aDesc.myProbeCountX * aDesc.myProbeCountY;
    const uint32_t height = aDesc.myProbeCountZ;
    if ( aState.myImagesReady && aState.myAtlasWidth == width && aState.myAtlasHeight == height ) {
        return true;
    }

    DestroyDdgiImagesOnly( aDevice, aState );

    Rhi::TextureDesc irradianceDesc{};
    irradianceDesc.myWidth   = width;
    irradianceDesc.myHeight  = height;
    irradianceDesc.myFormat  = Rhi_Format::RGBA16_Sfloat;
    irradianceDesc.myUsage   = Rhi_ImageUsage::Storage | Rhi_ImageUsage::Sampled;
    irradianceDesc.myMemory  = Rhi_MemoryUsage::GpuOnly;
    aState.myIrradianceAtlas = Rhi::DeviceCreateTexture( aDevice, irradianceDesc );

    Rhi::TextureDesc visibilityDesc{};
    visibilityDesc.myWidth   = width;
    visibilityDesc.myHeight  = height;
    visibilityDesc.myFormat  = Rhi_Format::R16_Sfloat;
    visibilityDesc.myUsage   = Rhi_ImageUsage::Storage | Rhi_ImageUsage::Sampled;
    visibilityDesc.myMemory  = Rhi_MemoryUsage::GpuOnly;
    aState.myVisibilityAtlas = Rhi::DeviceCreateTexture( aDevice, visibilityDesc );

    if ( !aState.myIrradianceAtlas || !aState.myVisibilityAtlas ) {
        DestroyDdgiImagesOnly( aDevice, aState );
        return false;
    }

    // Deferred always samples these as SHADER_READ_ONLY (even when DDGI is off) — leave UNDEFINED and validation fails.
    Rhi::DeviceTransitionTextureLayout( aDevice, aState.myIrradianceAtlas, Rhi_ImageLayout::Undefined, Rhi_ImageLayout::ShaderReadOnly );
    Rhi::DeviceTransitionTextureLayout( aDevice, aState.myVisibilityAtlas, Rhi_ImageLayout::Undefined, Rhi_ImageLayout::ShaderReadOnly );

    aState.myAtlasWidth  = width;
    aState.myAtlasHeight = height;
    aState.myImagesReady = true;
    return true;
}

void DestroyImages( Rhi_Device& aDevice, PassState& aState ) {
    DestroyDdgiImagesOnly( aDevice, aState );
}

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || !aState.myImagesReady || !aDesc.myWorldPos || !aDesc.myNormalRoughness ) {
        return;
    }

    const uint32_t frames = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    for ( uint32_t i = 0; i < frames; ++i ) {
        const std::array< Rhi::DescriptorImageWrite, 4 > writes = { {
            { aState.mySets[ i ], 0, Rhi_DescriptorType::StorageImage, {}, aState.myIrradianceAtlas, Rhi_ImageLayout::General },
            { aState.mySets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myVisibilityAtlas, Rhi_ImageLayout::General },
            { aState.mySets[ i ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myWorldPos, Rhi_ImageLayout::ShaderReadOnly },
            { aState.mySets[ i ], 3, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myNormalRoughness, Rhi_ImageLayout::ShaderReadOnly },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, writes.data(), static_cast< uint32_t >( writes.size() ) );
    }
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

#include "Gfx_DeferredLightingPass.h"

#include <array>

namespace Gfx_DeferredLightingPass {

bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( aState.myPipelineReady && aState.myPipeline ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myVertSpirv == nullptr || aDesc.myVertSpirvBytes == 0 || aDesc.myFragSpirv == nullptr || aDesc.myFragSpirvBytes == 0
         || !aDesc.myRenderPass ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 20 > bindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // albedo
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // normalRoughness
        { 2, Rhi_DescriptorType::StorageBuffer, 1, Rhi_ShaderStage::Fragment },          // cluster lights
        { 3, Rhi_DescriptorType::StorageBuffer, 1, Rhi_ShaderStage::Fragment },          // cluster lists
        { 4, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // depth
        { 5, Rhi_DescriptorType::UniformBuffer, 1, Rhi_ShaderStage::Fragment },          // lighting globals
        { 6, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // shadow compare
        { 7, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // IBL irradiance
        { 8, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // IBL prefilter
        { 9, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },   // BRDF LUT
        { 10, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // sky
        { 11, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // shadow depth read
        { 12, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // world pos
        { 13, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // ao / contact soft
        { 14, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // hiZ
        { 15, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // ddgi irradiance
        { 16, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // ddgi visibility
        { 17, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // ssr
        { 18, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // bent normal
        { 19, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },  // motion vector
    } };
    Rhi::DescriptorSetLayoutDesc                            setLayoutDesc{};
    setLayoutDesc.myBindings     = bindings.data();
    setLayoutDesc.myBindingCount = static_cast< uint32_t >( bindings.size() );
    aState.mySetLayout           = Rhi::DeviceCreateDescriptorSetLayout( aDevice, setLayoutDesc );
    if ( !aState.mySetLayout ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Fragment;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = static_cast< uint32_t >( sizeof( Gpu_DeferredLightingPushConstants ) );

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
    const std::array< Rhi::DescriptorPoolSize, 3 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 20u },
        { Rhi_DescriptorType::StorageBuffer, frames * 2u },
        { Rhi_DescriptorType::UniformBuffer, frames },
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

    Rhi_ShaderModule vert = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myVertSpirv, aDesc.myVertSpirvBytes );
    Rhi_ShaderModule frag = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myFragSpirv, aDesc.myFragSpirvBytes );
    if ( !vert || !frag ) {
        Rhi::DeviceDestroyShaderModule( aDevice, vert );
        Rhi::DeviceDestroyShaderModule( aDevice, frag );
        DestroyPipeline( aDevice, aState );
        return false;
    }

    Rhi::GraphicsPipelineDesc pipeDesc{};
    pipeDesc.myVertexShader           = vert;
    pipeDesc.myFragmentShader         = frag;
    pipeDesc.myLayout                 = aState.myLayout;
    pipeDesc.myRenderPass             = aDesc.myRenderPass;
    pipeDesc.myColorAttachmentCount   = 1;
    pipeDesc.mySampleCount            = aDesc.mySampleCount > 0 ? aDesc.mySampleCount : 1u;
    pipeDesc.myCullMode               = Rhi_CullMode::None;
    pipeDesc.myDepthTestEnable        = false;
    pipeDesc.myDepthWriteEnable       = false;
    pipeDesc.myBlendEnable            = false;
    pipeDesc.myDynamicViewportScissor = true;

    aState.myPipeline = Rhi::DeviceCreateGraphicsPipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, vert );
    Rhi::DeviceDestroyShaderModule( aDevice, frag );
    if ( !aState.myPipeline ) {
        DestroyPipeline( aDevice, aState );
        return false;
    }

    aState.myPipelineReady = true;
    return true;
}

bool CreateOrRecreateGraphicsPipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myLayout || !aDesc.myRenderPass || aDesc.myVertSpirv == nullptr || aDesc.myVertSpirvBytes == 0 || aDesc.myFragSpirv == nullptr
         || aDesc.myFragSpirvBytes == 0 ) {
        return false;
    }

    if ( aState.myPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myPipeline );
        aState.myPipelineReady = false;
    }

    Rhi_ShaderModule vert = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myVertSpirv, aDesc.myVertSpirvBytes );
    Rhi_ShaderModule frag = Rhi::DeviceCreateShaderModule( aDevice, aDesc.myFragSpirv, aDesc.myFragSpirvBytes );
    if ( !vert || !frag ) {
        Rhi::DeviceDestroyShaderModule( aDevice, vert );
        Rhi::DeviceDestroyShaderModule( aDevice, frag );
        return false;
    }

    Rhi::GraphicsPipelineDesc pipeDesc{};
    pipeDesc.myVertexShader           = vert;
    pipeDesc.myFragmentShader         = frag;
    pipeDesc.myLayout                 = aState.myLayout;
    pipeDesc.myRenderPass             = aDesc.myRenderPass;
    pipeDesc.myColorAttachmentCount   = 1;
    pipeDesc.mySampleCount            = aDesc.mySampleCount > 0 ? aDesc.mySampleCount : 1u;
    pipeDesc.myCullMode               = Rhi_CullMode::None;
    pipeDesc.myDepthTestEnable        = false;
    pipeDesc.myDepthWriteEnable       = false;
    pipeDesc.myBlendEnable            = false;
    pipeDesc.myDynamicViewportScissor = true;

    aState.myPipeline = Rhi::DeviceCreateGraphicsPipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, vert );
    Rhi::DeviceDestroyShaderModule( aDevice, frag );
    if ( !aState.myPipeline ) {
        return false;
    }

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
    DestroyPipeline( aDevice, aState );
}

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || aDesc.myFrameIndex >= aState.mySets.size() || !aState.mySets[ aDesc.myFrameIndex ] ) {
        return;
    }
    const Rhi_DescriptorSet set = aState.mySets[ aDesc.myFrameIndex ];

    // Validation rejects null imageView on COMBINED_IMAGE_SAMPLER; fall back to albedo when a feature target is absent.
    auto orFallback = [ &aDesc ]( Rhi_Texture aTex ) { return aTex ? aTex : aDesc.myFallback; };

    const std::array< Rhi::DescriptorImageWrite, 17 > imageWrites = { {
        { set, 0, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myAlbedo, Rhi_ImageLayout::ShaderReadOnly },
        { set, 1, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myNormalRoughness, Rhi_ImageLayout::ShaderReadOnly },
        { set, 4, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myDepth, Rhi_ImageLayout::DepthStencilReadOnly },
        { set, 6, Rhi_DescriptorType::CombinedImageSampler, aDesc.myShadowCompareSampler, orFallback( aDesc.myShadowDepth ), Rhi_ImageLayout::DepthStencilReadOnly },
        { set, 7, Rhi_DescriptorType::CombinedImageSampler, aDesc.myIblCubemapSampler, orFallback( aDesc.myIrradianceIbl ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 8, Rhi_DescriptorType::CombinedImageSampler, aDesc.myIblCubemapSampler, orFallback( aDesc.myPrefilterIbl ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 9, Rhi_DescriptorType::CombinedImageSampler, aDesc.myBrdfLutSampler, orFallback( aDesc.myBrdfLut ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 10, Rhi_DescriptorType::CombinedImageSampler, aDesc.myIblCubemapSampler, orFallback( aDesc.mySky ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 11, Rhi_DescriptorType::CombinedImageSampler, aDesc.myShadowDepthReadSampler, orFallback( aDesc.myShadowDepthRead ), Rhi_ImageLayout::DepthStencilReadOnly },
        { set, 12, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myWorldPos, Rhi_ImageLayout::ShaderReadOnly },
        { set, 13, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, orFallback( aDesc.myAo ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 14, Rhi_DescriptorType::CombinedImageSampler, aDesc.myHiZSampler, orFallback( aDesc.myHiZ ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 15, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, orFallback( aDesc.myDdgiIrradiance ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 16, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, orFallback( aDesc.myDdgiVisibility ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 17, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, orFallback( aDesc.mySsr ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 18, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, orFallback( aDesc.myBentNormal ), Rhi_ImageLayout::ShaderReadOnly },
        { set, 19, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, aDesc.myMotionVector, Rhi_ImageLayout::ShaderReadOnly },
    } };
    Rhi::DeviceUpdateDescriptorImages( aDevice, imageWrites.data(), static_cast< uint32_t >( imageWrites.size() ) );

    const std::array< Rhi::DescriptorBufferWrite, 3 > bufferWrites = { {
        { set, 2, Rhi_DescriptorType::StorageBuffer, aDesc.myLightsBuffer, 0, aDesc.myLightsRangeBytes },
        { set, 3, Rhi_DescriptorType::StorageBuffer, aDesc.myClusterListBuffer, 0, 0 },
        { set, 5, Rhi_DescriptorType::UniformBuffer, aDesc.myLightingGlobals, aDesc.myLightingGlobalsOffsetBytes, aDesc.myLightingGlobalsRangeBytes },
    } };
    Rhi::DeviceUpdateDescriptorBuffers( aDevice, bufferWrites.data(), static_cast< uint32_t >( bufferWrites.size() ) );
}

void UpdateAoBinding( Rhi_Device& aDevice, const AoBindingUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || aDesc.myFrameIndex >= aState.mySets.size() || !aState.mySets[ aDesc.myFrameIndex ] ) {
        return;
    }
    const Rhi_Texture               ao = aDesc.myAo ? aDesc.myAo : aDesc.myFallback;
    const Rhi::DescriptorImageWrite write{ aState.mySets[ aDesc.myFrameIndex ], 13, Rhi_DescriptorType::CombinedImageSampler, aState.myGBufferSampler, ao,
                                           Rhi_ImageLayout::ShaderReadOnly };
    Rhi::DeviceUpdateDescriptorImages( aDevice, &write, 1 );
}

void RecordDraw( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput ) {
    if ( !Rhi::CommandListIsRecordingReady( aCmd ) ) {
        return;
    }

    Rhi::CommandListBindPipeline( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myPipeline );
    Rhi::CommandListBindDescriptorSet( aCmd, Rhi_PipelineBindPoint::Graphics, aGpu.myLayout, 0, aGpu.mySet );
    Rhi::CommandListPushConstants( aCmd, aGpu.myLayout, Rhi_ShaderStage::Fragment, 0, sizeof( aInput.myPush ), &aInput.myPush );

    if ( aInput.myDebugLabels ) {
        Rhi::CommandListBeginDebugLabel( aCmd, "Pass=DeferredLighting" );
    }
    Rhi::CommandListDraw( aCmd, 3, 1, 0, 0 );
    if ( aInput.myDebugLabels ) {
        Rhi::CommandListEndDebugLabel( aCmd );
    }
}

}  // namespace Gfx_DeferredLightingPass

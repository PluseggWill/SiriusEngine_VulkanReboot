#include "Gfx_PostProcessPass.h"

#include "../Rhi/Rhi_Device.h"

#include <algorithm>
#include <array>

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

bool CreateComputeLayout( Rhi_Device& aDevice, Rhi_DescriptorSetLayout aSetLayout, uint32_t aPushBytes, Rhi_PipelineLayout& aOut ) {
    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Compute;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = aPushBytes;

    Rhi::PipelineLayoutDesc layoutDesc{};
    layoutDesc.mySetLayouts     = &aSetLayout;
    layoutDesc.mySetLayoutCount = 1;
    layoutDesc.myPushRanges     = &push;
    layoutDesc.myPushRangeCount = 1;
    aOut                        = Rhi::DeviceCreatePipelineLayout( aDevice, layoutDesc );
    return static_cast< bool >( aOut );
}

Rhi_Texture CreateHdrTexture( Rhi_Device& aDevice, uint32_t aWidth, uint32_t aHeight, Rhi_ImageUsage aUsage ) {
    Rhi::TextureDesc desc{};
    desc.myWidth  = aWidth;
    desc.myHeight = aHeight;
    desc.myFormat = Rhi_Format::RGBA16_Sfloat;
    desc.myUsage  = aUsage;
    desc.myMemory = Rhi_MemoryUsage::GpuOnly;
    return Rhi::DeviceCreateTexture( aDevice, desc );
}

void DestroyImagesOnly( Rhi_Device& aDevice, Gfx_PostProcessPass::PassState& aState ) {
    Rhi::DeviceDestroyTexture( aDevice, aState.mySceneColor );
    Rhi::DeviceDestroyTexture( aDevice, aState.myTaaResolved );
    Rhi::DeviceDestroyTexture( aDevice, aState.myTaaHistory0 );
    Rhi::DeviceDestroyTexture( aDevice, aState.myTaaHistory1 );
    Rhi::DeviceDestroyTexture( aDevice, aState.myBloomPing );
    Rhi::DeviceDestroyTexture( aDevice, aState.myBloomPong );
    aState.myWidth       = 0;
    aState.myHeight      = 0;
    aState.myBloomWidth  = 0;
    aState.myBloomHeight = 0;
    aState.myImagesReady = false;
}

}  // namespace

namespace Gfx_PostProcessPass {

void DestroyComputeResources( Rhi_Device& aDevice, PassState& aState );

bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState ) {
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myWidth == 0 || aDesc.myHeight == 0 ) {
        return false;
    }
    const uint32_t bloomW = std::max( 1u, aDesc.myWidth / 2u );
    const uint32_t bloomH = std::max( 1u, aDesc.myHeight / 2u );
    if ( aState.myImagesReady && aState.myWidth == aDesc.myWidth && aState.myHeight == aDesc.myHeight && aState.myBloomWidth == bloomW && aState.myBloomHeight == bloomH ) {
        return true;
    }

    DestroyImagesOnly( aDevice, aState );

    const Rhi_ImageUsage sceneUsage    = Rhi_ImageUsage::ColorAttachment | Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage | Rhi_ImageUsage::TransferSrc;
    const Rhi_ImageUsage resolvedUsage = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage | Rhi_ImageUsage::TransferSrc;
    const Rhi_ImageUsage historyUsage  = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage | Rhi_ImageUsage::TransferDst | Rhi_ImageUsage::TransferSrc;
    const Rhi_ImageUsage bloomUsage    = Rhi_ImageUsage::Sampled | Rhi_ImageUsage::Storage;

    aState.mySceneColor  = CreateHdrTexture( aDevice, aDesc.myWidth, aDesc.myHeight, sceneUsage );
    aState.myTaaResolved = CreateHdrTexture( aDevice, aDesc.myWidth, aDesc.myHeight, resolvedUsage );
    aState.myTaaHistory0 = CreateHdrTexture( aDevice, aDesc.myWidth, aDesc.myHeight, historyUsage );
    aState.myTaaHistory1 = CreateHdrTexture( aDevice, aDesc.myWidth, aDesc.myHeight, historyUsage );
    aState.myBloomPing   = CreateHdrTexture( aDevice, bloomW, bloomH, bloomUsage );
    aState.myBloomPong   = CreateHdrTexture( aDevice, bloomW, bloomH, bloomUsage );

    if ( !aState.mySceneColor || !aState.myTaaResolved || !aState.myTaaHistory0 || !aState.myTaaHistory1 || !aState.myBloomPing || !aState.myBloomPong ) {
        DestroyImagesOnly( aDevice, aState );
        return false;
    }

    aState.myWidth       = aDesc.myWidth;
    aState.myHeight      = aDesc.myHeight;
    aState.myBloomWidth  = bloomW;
    aState.myBloomHeight = bloomH;
    aState.myImagesReady = true;
    return true;
}

void DestroyImages( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImagesOnly( aDevice, aState );
}

bool CreateComputeResources( Rhi_Device& aDevice, const ComputeInitDesc& aDesc, PassState& aState ) {
    if ( aState.myComputeReady ) {
        return true;
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || aDesc.myThresholdSpirv == nullptr || aDesc.myThresholdSpirvBytes == 0 || aDesc.myBlurSpirv == nullptr
         || aDesc.myBlurSpirvBytes == 0 || aDesc.myTaaSpirv == nullptr || aDesc.myTaaSpirvBytes == 0 ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 2 > thresholdBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           thresholdLayoutDesc{};
    thresholdLayoutDesc.myBindings     = thresholdBindings.data();
    thresholdLayoutDesc.myBindingCount = static_cast< uint32_t >( thresholdBindings.size() );
    aState.myThresholdSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, thresholdLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 2 > blurBindings = { {
        { 0, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           blurLayoutDesc{};
    blurLayoutDesc.myBindings     = blurBindings.data();
    blurLayoutDesc.myBindingCount = static_cast< uint32_t >( blurBindings.size() );
    aState.myBlurSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, blurLayoutDesc );

    const std::array< Rhi::DescriptorSetLayoutBinding, 4 > taaBindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 2, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Compute },
        { 3, Rhi_DescriptorType::StorageImage, 1, Rhi_ShaderStage::Compute },
    } };
    Rhi::DescriptorSetLayoutDesc                           taaLayoutDesc{};
    taaLayoutDesc.myBindings     = taaBindings.data();
    taaLayoutDesc.myBindingCount = static_cast< uint32_t >( taaBindings.size() );
    aState.myTaaSetLayout        = Rhi::DeviceCreateDescriptorSetLayout( aDevice, taaLayoutDesc );

    if ( !aState.myThresholdSetLayout || !aState.myBlurSetLayout || !aState.myTaaSetLayout
         || !CreateComputeLayout( aDevice, aState.myThresholdSetLayout, 16, aState.myThresholdLayout )
         || !CreateComputeLayout( aDevice, aState.myBlurSetLayout, 8, aState.myBlurLayout )
         || !CreateComputeLayout( aDevice, aState.myTaaSetLayout, sizeof( TaaPush ), aState.myTaaLayout ) ) {
        DestroyComputeResources( aDevice, aState );
        return false;
    }

    if ( !CreateOneCompute( aDevice, aDesc.myThresholdSpirv, aDesc.myThresholdSpirvBytes, aState.myThresholdLayout, aState.myThresholdPipeline )
         || !CreateOneCompute( aDevice, aDesc.myBlurSpirv, aDesc.myBlurSpirvBytes, aState.myBlurLayout, aState.myBlurPipeline )
         || !CreateOneCompute( aDevice, aDesc.myTaaSpirv, aDesc.myTaaSpirvBytes, aState.myTaaLayout, aState.myTaaPipeline ) ) {
        DestroyComputeResources( aDevice, aState );
        return false;
    }

    Rhi::SamplerDesc samplerDesc{};
    samplerDesc.myMagFilter = Rhi_Filter::Linear;
    samplerDesc.myMinFilter = Rhi_Filter::Linear;
    samplerDesc.myAddressU  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressV  = Rhi_AddressMode::ClampToEdge;
    samplerDesc.myAddressW  = Rhi_AddressMode::ClampToEdge;
    aState.mySceneSampler   = Rhi::DeviceCreateSampler( aDevice, samplerDesc );
    if ( !aState.mySceneSampler ) {
        DestroyComputeResources( aDevice, aState );
        return false;
    }

    const uint32_t                                 frames    = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const std::array< Rhi::DescriptorPoolSize, 2 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 8u },
        { Rhi_DescriptorType::StorageImage, frames * 6u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames * 4u;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    aState.myPool            = Rhi::DeviceCreateDescriptorPool( aDevice, poolDesc );
    if ( !aState.myPool ) {
        DestroyComputeResources( aDevice, aState );
        return false;
    }

    for ( uint32_t i = 0; i < frames; ++i ) {
        aState.myThresholdSets[ i ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myThresholdSetLayout );
        aState.myBlurHorizSets[ i ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myBlurSetLayout );
        aState.myBlurVertSets[ i ]  = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myBlurSetLayout );
        aState.myTaaSets[ i ]       = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myPool, aState.myTaaSetLayout );
        if ( !aState.myThresholdSets[ i ] || !aState.myBlurHorizSets[ i ] || !aState.myBlurVertSets[ i ] || !aState.myTaaSets[ i ] ) {
            DestroyComputeResources( aDevice, aState );
            return false;
        }
    }
    aState.mySetsAllocated = true;
    aState.myComputeReady  = true;
    return true;
}

void UpdateComputeDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.mySetsAllocated || !aState.myImagesReady || !aState.mySceneSampler ) {
        return;
    }

    const uint32_t    frames      = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const Rhi_Texture historyRead = ( aDesc.myTaaHistoryReadIndex % 2u == 0u ) ? aState.myTaaHistory0 : aState.myTaaHistory1;
    const uint32_t    begin       = ( aDesc.myFrameIndex == 0xffffffffu ) ? 0u : aDesc.myFrameIndex;
    const uint32_t    end         = ( aDesc.myFrameIndex == 0xffffffffu ) ? frames : ( aDesc.myFrameIndex + 1u );
    if ( begin >= frames || begin >= end ) {
        return;
    }

    for ( uint32_t i = begin; i < end && i < frames; ++i ) {
        const std::array< Rhi::DescriptorImageWrite, 2 > threshold = { {
            { aState.myThresholdSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, aState.mySceneColor, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myThresholdSets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myBloomPing, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, threshold.data(), static_cast< uint32_t >( threshold.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 2 > blurH = { {
            { aState.myBlurHorizSets[ i ], 0, Rhi_DescriptorType::StorageImage, {}, aState.myBloomPing, Rhi_ImageLayout::General },
            { aState.myBlurHorizSets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myBloomPong, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, blurH.data(), static_cast< uint32_t >( blurH.size() ) );

        const std::array< Rhi::DescriptorImageWrite, 2 > blurV = { {
            { aState.myBlurVertSets[ i ], 0, Rhi_DescriptorType::StorageImage, {}, aState.myBloomPong, Rhi_ImageLayout::General },
            { aState.myBlurVertSets[ i ], 1, Rhi_DescriptorType::StorageImage, {}, aState.myBloomPing, Rhi_ImageLayout::General },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, blurV.data(), static_cast< uint32_t >( blurV.size() ) );

        if ( aDesc.myMotionVector ) {
            const std::array< Rhi::DescriptorImageWrite, 4 > taa = { {
                { aState.myTaaSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, aState.mySceneColor, Rhi_ImageLayout::ShaderReadOnly },
                { aState.myTaaSets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, historyRead, Rhi_ImageLayout::ShaderReadOnly },
                { aState.myTaaSets[ i ], 2, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, aDesc.myMotionVector, Rhi_ImageLayout::ShaderReadOnly },
                { aState.myTaaSets[ i ], 3, Rhi_DescriptorType::StorageImage, {}, aState.myTaaResolved, Rhi_ImageLayout::General },
            } };
            Rhi::DeviceUpdateDescriptorImages( aDevice, taa.data(), static_cast< uint32_t >( taa.size() ) );
        }
    }
}

void DestroyComputeResources( Rhi_Device& aDevice, PassState& aState ) {
    for ( auto& set : aState.myThresholdSets ) {
        set = {};
    }
    for ( auto& set : aState.myBlurHorizSets ) {
        set = {};
    }
    for ( auto& set : aState.myBlurVertSets ) {
        set = {};
    }
    for ( auto& set : aState.myTaaSets ) {
        set = {};
    }
    aState.mySetsAllocated = false;

    if ( aState.myThresholdPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myThresholdPipeline );
    }
    if ( aState.myBlurPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myBlurPipeline );
    }
    if ( aState.myTaaPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myTaaPipeline );
    }
    if ( aState.myThresholdLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myThresholdLayout );
    }
    if ( aState.myBlurLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myBlurLayout );
    }
    if ( aState.myTaaLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myTaaLayout );
    }
    if ( aState.myPool ) {
        Rhi::DeviceDestroyDescriptorPool( aDevice, aState.myPool );
    }
    if ( aState.myThresholdSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myThresholdSetLayout );
    }
    if ( aState.myBlurSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myBlurSetLayout );
    }
    if ( aState.myTaaSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myTaaSetLayout );
    }
    if ( aState.mySceneSampler ) {
        Rhi::DeviceDestroySampler( aDevice, aState.mySceneSampler );
    }
    aState.myComputeReady = false;
}

void DestroyTonemapResources( Rhi_Device& aDevice, PassState& aState ) {
    for ( auto& set : aState.myTonemapSets ) {
        set = {};
    }
    if ( aState.myTonemapPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myTonemapPipeline );
    }
    if ( aState.myTonemapLayout ) {
        Rhi::DeviceDestroyPipelineLayout( aDevice, aState.myTonemapLayout );
    }
    if ( aState.myTonemapPool ) {
        Rhi::DeviceDestroyDescriptorPool( aDevice, aState.myTonemapPool );
    }
    if ( aState.myTonemapSetLayout ) {
        Rhi::DeviceDestroyDescriptorSetLayout( aDevice, aState.myTonemapSetLayout );
    }
    aState.myTonemapReady         = false;
    aState.myTonemapPipelineReady = false;
}

bool CreateTonemapResources( Rhi_Device& aDevice, const TonemapInitDesc& aDesc, PassState& aState ) {
    if ( aState.myTonemapReady ) {
        return CreateOrRecreateTonemapPipeline( aDevice, aDesc, aState );
    }
    if ( !Rhi::DeviceHasLogicalDevice( aDevice ) || !aState.mySceneSampler ) {
        return false;
    }

    const std::array< Rhi::DescriptorSetLayoutBinding, 2 > bindings = { {
        { 0, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },
        { 1, Rhi_DescriptorType::CombinedImageSampler, 1, Rhi_ShaderStage::Fragment },
    } };
    Rhi::DescriptorSetLayoutDesc                           layoutDesc{};
    layoutDesc.myBindings     = bindings.data();
    layoutDesc.myBindingCount = static_cast< uint32_t >( bindings.size() );
    aState.myTonemapSetLayout = Rhi::DeviceCreateDescriptorSetLayout( aDevice, layoutDesc );
    if ( !aState.myTonemapSetLayout ) {
        DestroyTonemapResources( aDevice, aState );
        return false;
    }

    Rhi::PushConstantRangeDesc push{};
    push.myStages      = Rhi_ShaderStage::Fragment;
    push.myOffsetBytes = 0;
    push.mySizeBytes   = sizeof( TonemapPush );

    Rhi::PipelineLayoutDesc pipeLayoutDesc{};
    pipeLayoutDesc.mySetLayouts     = &aState.myTonemapSetLayout;
    pipeLayoutDesc.mySetLayoutCount = 1;
    pipeLayoutDesc.myPushRanges     = &push;
    pipeLayoutDesc.myPushRangeCount = 1;
    aState.myTonemapLayout          = Rhi::DeviceCreatePipelineLayout( aDevice, pipeLayoutDesc );
    if ( !aState.myTonemapLayout ) {
        DestroyTonemapResources( aDevice, aState );
        return false;
    }

    const uint32_t                                 frames    = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const std::array< Rhi::DescriptorPoolSize, 1 > poolSizes = { {
        { Rhi_DescriptorType::CombinedImageSampler, frames * 2u },
    } };
    Rhi::DescriptorPoolDesc                        poolDesc{};
    poolDesc.myMaxSets       = frames;
    poolDesc.myPoolSizes     = poolSizes.data();
    poolDesc.myPoolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    aState.myTonemapPool     = Rhi::DeviceCreateDescriptorPool( aDevice, poolDesc );
    if ( !aState.myTonemapPool ) {
        DestroyTonemapResources( aDevice, aState );
        return false;
    }

    for ( uint32_t i = 0; i < frames; ++i ) {
        aState.myTonemapSets[ i ] = Rhi::DeviceAllocateDescriptorSet( aDevice, aState.myTonemapPool, aState.myTonemapSetLayout );
        if ( !aState.myTonemapSets[ i ] ) {
            DestroyTonemapResources( aDevice, aState );
            return false;
        }
    }

    aState.myTonemapReady = true;
    return CreateOrRecreateTonemapPipeline( aDevice, aDesc, aState );
}

bool CreateOrRecreateTonemapPipeline( Rhi_Device& aDevice, const TonemapInitDesc& aDesc, PassState& aState ) {
    if ( !aState.myTonemapReady || !aState.myTonemapLayout || !aDesc.myRenderPass || aDesc.myVertSpirv == nullptr || aDesc.myVertSpirvBytes == 0
         || aDesc.myFragSpirv == nullptr || aDesc.myFragSpirvBytes == 0 ) {
        return false;
    }

    if ( aState.myTonemapPipeline ) {
        Rhi::DeviceDestroyPipeline( aDevice, aState.myTonemapPipeline );
        aState.myTonemapPipelineReady = false;
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
    pipeDesc.myLayout                 = aState.myTonemapLayout;
    pipeDesc.myRenderPass             = aDesc.myRenderPass;
    pipeDesc.myColorAttachmentCount   = 1;
    pipeDesc.mySampleCount            = aDesc.mySampleCount > 0 ? aDesc.mySampleCount : 1u;
    pipeDesc.myCullMode               = Rhi_CullMode::None;
    pipeDesc.myDepthTestEnable        = false;
    pipeDesc.myDepthWriteEnable       = false;
    pipeDesc.myBlendEnable            = false;
    pipeDesc.myDynamicViewportScissor = true;

    aState.myTonemapPipeline = Rhi::DeviceCreateGraphicsPipeline( aDevice, pipeDesc );
    Rhi::DeviceDestroyShaderModule( aDevice, vert );
    Rhi::DeviceDestroyShaderModule( aDevice, frag );
    if ( !aState.myTonemapPipeline ) {
        return false;
    }

    aState.myTonemapPipelineReady = true;
    return true;
}

void UpdateTonemapDescriptors( Rhi_Device& aDevice, const TonemapDescriptorUpdateDesc& aDesc, PassState& aState ) {
    if ( !aState.myTonemapReady || !aState.myImagesReady || !aState.mySceneSampler ) {
        return;
    }

    const uint32_t    frames = aDesc.myFramesInFlight > 0u ? aDesc.myFramesInFlight : kMaxFramesInFlight;
    const Rhi_Texture scene  = ( aDesc.myUseTaaResolved && aState.myTaaResolved ) ? aState.myTaaResolved : aState.mySceneColor;
    const uint32_t    begin  = ( aDesc.myFrameIndex == 0xffffffffu ) ? 0u : aDesc.myFrameIndex;
    const uint32_t    end    = ( aDesc.myFrameIndex == 0xffffffffu ) ? frames : ( aDesc.myFrameIndex + 1u );
    if ( begin >= frames || begin >= end || !scene || !aState.myBloomPing ) {
        return;
    }

    for ( uint32_t i = begin; i < end && i < frames; ++i ) {
        const std::array< Rhi::DescriptorImageWrite, 2 > writes = { {
            { aState.myTonemapSets[ i ], 0, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, scene, Rhi_ImageLayout::ShaderReadOnly },
            { aState.myTonemapSets[ i ], 1, Rhi_DescriptorType::CombinedImageSampler, aState.mySceneSampler, aState.myBloomPing, Rhi_ImageLayout::ShaderReadOnly },
        } };
        Rhi::DeviceUpdateDescriptorImages( aDevice, writes.data(), static_cast< uint32_t >( writes.size() ) );
    }
}

void Destroy( Rhi_Device& aDevice, PassState& aState ) {
    DestroyImages( aDevice, aState );
    DestroyTonemapResources( aDevice, aState );
    DestroyComputeResources( aDevice, aState );
}

bool CreateComputePipelines( Rhi_Device& aDevice, const ComputePipelinesInitDesc& aDesc, ComputePassState& aState ) {
    return CreateComputeResources( aDevice, aDesc, aState );
}

void DestroyComputePipelines( Rhi_Device& aDevice, ComputePassState& aState ) {
    DestroyComputeResources( aDevice, aState );
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

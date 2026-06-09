#include "Vk_DescriptorSystem.h"

#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"
#include "Vk_ResourceContext.h"
#include "Vk_ShaderEffectMeta.h"

#include "../Gfx/Gfx_RenderView.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Logger.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {

constexpr VkFormat kBindlessDefaultTextureFormat = VK_FORMAT_R8G8B8A8_SRGB;

uint32_t WithPoolHeadroom( uint32_t aCount ) {
    return static_cast< uint32_t >( std::ceil( static_cast< float >( aCount ) * 1.2f ) ) + 4;
}

struct DescriptorPoolPlan {
    std::array< VkDescriptorPoolSize, 5 > myPoolSizes{};
    uint32_t                              myMaxSets = 0;
};

DescriptorPoolPlan ComputeDescriptorPoolPlan( const Vk_Core& aCore ) {
    // Sizes derived from scene manifest at load time; 20% headroom + policy caps fail before vkCreateDescriptorPool.
    const size_t materialCount = aCore.mySceneGpuCtx.myResourceTables.GetMaterialCount();
    const size_t textureCount  = aCore.mySceneGpuCtx.myResourceTables.GetTextureCount();

    if ( materialCount > VkDescriptorPolicy::kMaxSceneMaterials ) {
        throw std::runtime_error( "scene material count " + std::to_string( materialCount ) + " exceeds policy max "
                                  + std::to_string( VkDescriptorPolicy::kMaxSceneMaterials ) );
    }
    if ( textureCount > VkDescriptorPolicy::kMaxSceneTextures ) {
        throw std::runtime_error( "scene texture count " + std::to_string( textureCount ) + " exceeds policy max " + std::to_string( VkDescriptorPolicy::kMaxSceneTextures ) );
    }

    const bool bindless = aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless;

    const uint32_t frameSets    = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT ) * kGfxMaxRenderViews;
    const uint32_t objectSets   = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );
    const uint32_t materialSets = bindless ? 1u : static_cast< uint32_t >( materialCount );
    const uint32_t baseSets     = frameSets + objectSets + materialSets;

    const uint32_t uniformStatic    = WithPoolHeadroom( frameSets * 2 + static_cast< uint32_t >( materialCount ) );
    const uint32_t uniformDynamic   = WithPoolHeadroom( static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT ) );
    const uint32_t combinedSamplers = bindless ? WithPoolHeadroom( VkDescriptorPolicy::kMaxBindlessTextures ) : WithPoolHeadroom( static_cast< uint32_t >( materialCount ) );
    const uint32_t storageBuffers   = WithPoolHeadroom( bindless ? 2u : 1u );

    DescriptorPoolPlan plan{};
    plan.myPoolSizes[ 0 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    plan.myPoolSizes[ 0 ].descriptorCount = uniformStatic;
    plan.myPoolSizes[ 1 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    plan.myPoolSizes[ 1 ].descriptorCount = uniformDynamic;
    plan.myPoolSizes[ 2 ].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    plan.myPoolSizes[ 2 ].descriptorCount = combinedSamplers;
    plan.myPoolSizes[ 3 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    plan.myPoolSizes[ 3 ].descriptorCount = uniformDynamic;
    plan.myPoolSizes[ 4 ].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    plan.myPoolSizes[ 4 ].descriptorCount = storageBuffers;
    plan.myMaxSets                        = WithPoolHeadroom( baseSets );
    return plan;
}

}  // namespace

void Vk_DescriptorSystem::EnsureBindlessDefaultTexture( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myBindlessDefaultTexture.ImageView() != VK_NULL_HANDLE ) {
        return;
    }
    if ( !aCore.myDeviceCtx.myBindlessCaps.myDescriptorIndexingExtension ) {
        return;
    }

    Vk_ResourceContext context{};
    context.Bind( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myAllocator, aCore.myDeviceCtx.myPhysicalDevice, aCore.myDeviceCtx.myGraphicsQueue,
                  aCore.myDeviceCtx.myTransferQueue, aCore.myDeviceCtx.myGraphicsCommandPool, aCore.myDeviceCtx.myTransferCommandPool,
                  aCore.myDeviceCtx.myQueueFamilyIndices.myGraphicsFamily.value_or( 0 ), aCore.myDeviceCtx.myQueueFamilyIndices.myTransferFamily.value_or( 0 ) );

    static constexpr uint8_t      kWhiteRgba[ 4 ] = { 255, 255, 255, 255 };
    static constexpr uint32_t     kWidth          = 1;
    static constexpr uint32_t     kHeight         = 1;
    static constexpr VkDeviceSize kImageSize      = 4;

    Vk_AllocatedBuffer stagingBuffer{};
    context.CreateBuffer( kImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, true );

    void* mapped = nullptr;
    vmaMapMemory( aCore.myDeviceCtx.myAllocator, stagingBuffer.myAllocation, &mapped );
    std::memcpy( mapped, kWhiteRgba, sizeof( kWhiteRgba ) );
    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, stagingBuffer.myAllocation );

    const VkExtent3D extent{ kWidth, kHeight, 1 };
    Gfx_Texture&     texture = aCore.myDeviceCtx.myBindlessDefaultTexture;
    context.CreateImage( extent, kBindlessDefaultTextureFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, texture.AllocImage() );
    context.TransitionImageLayout( texture.Image(), kBindlessDefaultTextureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1 );
    context.CopyBufferToImage( stagingBuffer.myBuffer, texture.Image(), kWidth, kHeight );
    context.TransitionImageLayout( texture.Image(), kBindlessDefaultTextureFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    texture.ImageView() = context.CreateImageView( texture.Image(), kBindlessDefaultTextureFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1 );

    vmaDestroyBuffer( aCore.myDeviceCtx.myAllocator, stagingBuffer.myBuffer, stagingBuffer.myAllocation );

    const VmaAllocator  allocator  = aCore.myDeviceCtx.myAllocator;
    const VkDevice      device     = aCore.myDeviceCtx.myDevice;
    const VkImage       image      = texture.Image();
    const VmaAllocation allocation = texture.Allocation();
    const VkImageView   imageView  = texture.ImageView();
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ allocator, device, image, allocation, imageView ]() {
        vkDestroyImageView( device, imageView, nullptr );
        vmaDestroyImage( allocator, image, allocation );
    } );

    UtilLogger::Info( "BINDLESS", "Created 1x1 engine default texture for bindless array padding." );
}

void Vk_DescriptorSystem::InitDeviceLayouts( Vk_Core& aCore ) {
    EnsureBindlessDefaultTexture( aCore );
    CreateDescriptorSetLayout( aCore );
    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        CreateBindlessMaterialSetLayout( aCore );
        CreateBindlessPipelineLayout( aCore );
        // S2 2d: SPIR-V/contract already checked at build; catch hand-written layout drift at runtime on bindless GPUs.
        VkShaderEffectMeta::VerifyLitBindlessReflectionContract( aCore.EngineConfig() );
    }
    LogLayoutContract( aCore );
}

void Vk_DescriptorSystem::LogLayoutContract( const Vk_Core& aCore ) {
    const char* materialPath = aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? "bindless" : "batch";
    const char* set1Summary  = aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless ? "set1=textureArray+materialSSBO (TriangleFrag_Lit_Bindless.frag)"
                                                                                                   : "set1=texSampler+MaterialData UBO per materialId (TriangleFrag_Lit.frag)";
    UtilLogger::Info( "DESCRIPTOR", std::string( "layout contract: path=" ) + materialPath + " sets=0,1,2 (frame,material,object) "
                                        + "set0=camera+env (TriangleVertex/TriangleFrag_Lit) set2=GpuObjectData DYNAMIC (TriangleVertex) " + set1Summary
                                        + " rebuild=LoadScene/UnloadScene; swapchain=RefreshMaterialPipelines only" );
}

void Vk_DescriptorSystem::InitSceneDescriptors( Vk_Core& aCore ) {
    CreateTextureSampler( aCore );
    CreateDescriptorPool( aCore );
    CreateDescriptorSets( aCore );

    if ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Batch ) {
        CreateMaterialDescriptorSets( aCore );
    }
    else {
        CreateBindlessDescriptorResources( aCore );
    }
}

void Vk_DescriptorSystem::CreateDescriptorSetLayout( Vk_Core& aCore ) {
    // Lit batch Set 0/1/2 from reflection_lit.json + layout hash cache (S2 phase 2b).
    const LitBatchDescriptorSetLayouts layouts = VkShaderEffectMeta::AcquireLitBatchDescriptorSetLayouts( aCore );
    aCore.mySceneGpuCtx.myGlobalSetLayout      = layouts.myGlobalSetLayout;
    aCore.mySceneGpuCtx.myMaterialSetLayout    = layouts.myMaterialSetLayout;
    aCore.mySceneGpuCtx.myObjectSetLayout      = layouts.myObjectSetLayout;

    if ( aCore.EngineConfig().GetDescriptorLayoutMismatchTest() ) {
        VkShaderEffectMeta_RunLitBatchLayoutMismatchValidationTest( aCore.myDeviceCtx, aCore.mySceneGpuCtx, aCore );
    }
}

// POLICY_BINDLESS M2: hand-written until #18 codegen — must match TriangleFrag_Lit_Bindless.frag + DescriptorContract_LitBindless.json.
void Vk_DescriptorSystem::CreateBindlessMaterialSetLayout( Vk_Core& aCore ) {
    VkDescriptorSetLayoutBinding textureArrayBinding{};
    textureArrayBinding.binding         = eVk_BindlessTextureArrayBinding;
    textureArrayBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureArrayBinding.descriptorCount = VkDescriptorPolicy::kMaxBindlessTextures;
    textureArrayBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding tableBinding =
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, eVk_BindlessMaterialTableBinding );
    std::array< VkDescriptorSetLayoutBinding, 2 > bindings = { textureArrayBinding, tableBinding };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    VkDescriptorBindingFlags flags[ 2 ] = { VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, 0 };
    bindingFlagsInfo.bindingCount       = static_cast< uint32_t >( bindings.size() );
    bindingFlagsInfo.pBindingFlags      = flags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    layoutInfo.pNext        = &bindingFlagsInfo;
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &aCore.mySceneGpuCtx.myBindlessMaterialSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create bindless material descriptor set layout!" );
    }

    const VkDevice              device         = aCore.myDeviceCtx.myDevice;
    const VkDescriptorSetLayout bindlessLayout = aCore.mySceneGpuCtx.myBindlessMaterialSetLayout;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, bindlessLayout ]() { vkDestroyDescriptorSetLayout( device, bindlessLayout, nullptr ); } );
}

void Vk_DescriptorSystem::CreateBindlessPipelineLayout( Vk_Core& aCore ) {
    const std::array< VkDescriptorSetLayout, 3 > setLayouts = { aCore.mySceneGpuCtx.myGlobalSetLayout, aCore.mySceneGpuCtx.myBindlessMaterialSetLayout,
                                                                aCore.mySceneGpuCtx.myObjectSetLayout };
    VkPipelineLayoutCreateInfo                   layoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    layoutInfo.setLayoutCount                               = static_cast< uint32_t >( setLayouts.size() );
    layoutInfo.pSetLayouts                                  = setLayouts.data();
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &aCore.mySceneGpuCtx.myBindlessPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create bindless pipeline layout!" );
    }

    const VkDevice         device                 = aCore.myDeviceCtx.myDevice;
    const VkPipelineLayout bindlessPipelineLayout = aCore.mySceneGpuCtx.myBindlessPipelineLayout;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, bindlessPipelineLayout ]() { vkDestroyPipelineLayout( device, bindlessPipelineLayout, nullptr ); } );
}

void Vk_DescriptorSystem::CreateBindlessDescriptorResources( Vk_Core& aCore ) {
    const size_t textureCount  = aCore.mySceneGpuCtx.myResourceTables.GetTextureCount();
    const size_t materialCount = aCore.mySceneGpuCtx.myResourceTables.GetMaterialCount();

    const VkImageView paddingView = aCore.myDeviceCtx.myBindlessDefaultTexture.ImageView();
    if ( paddingView == VK_NULL_HANDLE ) {
        throw std::runtime_error( "CreateBindlessDescriptorResources: engine 1x1 default texture missing (call EnsureBindlessDefaultTexture at device init)" );
    }

    // Sized array [kMaxBindlessTextures] + PARTIALLY_BOUND on binding 0.
    // Slots [textureCount, max): engine 1x1 default (validation rejects null sampler writes on SDK 1.3.x).
    std::vector< VkDescriptorImageInfo > imageInfos( VkDescriptorPolicy::kMaxBindlessTextures );
    for ( size_t slot = 0; slot < VkDescriptorPolicy::kMaxBindlessTextures; ++slot ) {
        if ( slot < textureCount ) {
            const Gfx_Texture& texture     = aCore.mySceneGpuCtx.myResourceTables.GetTexture( static_cast< uint32_t >( slot ) );
            imageInfos[ slot ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[ slot ].imageView   = texture.ImageView();
            imageInfos[ slot ].sampler     = aCore.mySceneGpuCtx.myTextureSampler;
        }
        else {
            imageInfos[ slot ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[ slot ].imageView   = paddingView;
            imageInfos[ slot ].sampler     = aCore.mySceneGpuCtx.myTextureSampler;
        }
    }

    std::vector< GpuMaterialTableEntry > tableEntries( materialCount );
    for ( size_t materialId = 0; materialId < materialCount; ++materialId ) {
        const uint32_t      textureId = aCore.mySceneGpuCtx.myResourceTables.GetTextureIdForMaterial( static_cast< uint32_t >( materialId ) );
        const Gfx_Material& material  = aCore.mySceneGpuCtx.myResourceTables.GetMaterial( static_cast< uint32_t >( materialId ) );
        if ( textureId >= textureCount ) {
            throw std::runtime_error( "bindless material " + std::to_string( materialId ) + " textureId " + std::to_string( textureId ) + " >= textureCount "
                                      + std::to_string( textureCount ) );
        }
        tableEntries[ materialId ].myTextureIndex    = textureId;
        tableEntries[ materialId ].myRoughness       = material.myRoughness;
        tableEntries[ materialId ].myMetallic        = material.myMetallic;
        tableEntries[ materialId ].myAlpha           = material.myAlpha;
        tableEntries[ materialId ].myAlphaMode       = material.myAlphaMode;  // bindless frag mask discard
        tableEntries[ materialId ].myBaseColorFactor = material.myBaseColorFactor;
    }

    const VkDeviceSize tableSize = sizeof( GpuMaterialTableEntry ) * materialCount;
    aCore.CreateBuffer( tableSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, aCore.mySceneGpuCtx.myMaterialTableBuffer, true );
    void* mapped = nullptr;
    vmaMapMemory( aCore.myDeviceCtx.myAllocator, aCore.mySceneGpuCtx.myMaterialTableBuffer.myAllocation, &mapped );
    memcpy( mapped, tableEntries.data(), static_cast< size_t >( tableSize ) );
    vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, aCore.mySceneGpuCtx.myMaterialTableBuffer.myAllocation );

    const VmaAllocator  allocator       = aCore.myDeviceCtx.myAllocator;
    const VkBuffer      tableBuffer     = aCore.mySceneGpuCtx.myMaterialTableBuffer.myBuffer;
    const VmaAllocation tableAllocation = aCore.mySceneGpuCtx.myMaterialTableBuffer.myAllocation;
    aCore.GetSceneDeletionQueue().pushFunction( [ allocator, tableBuffer, tableAllocation ]() { vmaDestroyBuffer( allocator, tableBuffer, tableAllocation ); } );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = aCore.mySceneGpuCtx.myDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &aCore.mySceneGpuCtx.myBindlessMaterialSetLayout;
    if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &aCore.mySceneGpuCtx.myBindlessDescriptorSet ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate bindless descriptor set!" );
    }

    VkDescriptorBufferInfo tableBufferInfo{};
    tableBufferInfo.buffer = aCore.mySceneGpuCtx.myMaterialTableBuffer.myBuffer;
    tableBufferInfo.offset = 0;
    tableBufferInfo.range  = tableSize;

    std::array< VkWriteDescriptorSet, 2 > writes{};
    writes[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.mySceneGpuCtx.myBindlessDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos.data(),
                                                        eVk_BindlessTextureArrayBinding, static_cast< uint32_t >( VkDescriptorPolicy::kMaxBindlessTextures ) );
    writes[ 1 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.mySceneGpuCtx.myBindlessDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tableBufferInfo,
                                                        eVk_BindlessMaterialTableBinding, 1 );
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );

    UtilLogger::Info( "BINDLESS", "bindless set ready: textures=" + std::to_string( textureCount ) + " materials=" + std::to_string( materialCount )
                                      + " tableGeneration=" + std::to_string( aCore.mySceneGpuCtx.myResourceTables.GetMaterialTableGeneration() ) );
}

void Vk_DescriptorSystem::CreateDescriptorPool( Vk_Core& aCore ) {
    const DescriptorPoolPlan plan = ComputeDescriptorPoolPlan( aCore );

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( plan.myPoolSizes.size() );
    poolInfo.pPoolSizes    = plan.myPoolSizes.data();
    poolInfo.maxSets       = plan.myMaxSets;
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &aCore.mySceneGpuCtx.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create descriptor pool!" );
    }

    UtilLogger::Info( "DESCRIPTOR", "pool from manifest: maxSets=" + std::to_string( plan.myMaxSets )
                                        + " materials=" + std::to_string( aCore.mySceneGpuCtx.myResourceTables.GetMaterialCount() )
                                        + " textures=" + std::to_string( aCore.mySceneGpuCtx.myResourceTables.GetTextureCount() ) + " UBO="
                                        + std::to_string( plan.myPoolSizes[ 0 ].descriptorCount ) + " dynamicUBO=" + std::to_string( plan.myPoolSizes[ 1 ].descriptorCount )
                                        + " samplers=" + std::to_string( plan.myPoolSizes[ 2 ].descriptorCount ) );

    const VkDevice         device = aCore.myDeviceCtx.myDevice;
    const VkDescriptorPool pool   = aCore.mySceneGpuCtx.myDescriptorPool;
    aCore.GetSceneDeletionQueue().pushFunction( [ device, pool ]() { vkDestroyDescriptorPool( device, pool, nullptr ); } );
}

void Vk_DescriptorSystem::CreateTextureSampler( Vk_Core& aCore ) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties( aCore.myDeviceCtx.myPhysicalDevice, &properties );
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod                  = static_cast< float >( aCore.mySceneGpuCtx.myTextureImageMipLevels );
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &aCore.mySceneGpuCtx.myTextureSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create texture sampler!" );
    }

    const VkDevice  device  = aCore.myDeviceCtx.myDevice;
    const VkSampler sampler = aCore.mySceneGpuCtx.myTextureSampler;
    aCore.GetSceneDeletionQueue().pushFunction( [ device, sampler ]() { vkDestroySampler( device, sampler, nullptr ); } );
}

void Vk_DescriptorSystem::CreateDescriptorSets( Vk_Core& aCore ) {
    for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ ) {
        VkDescriptorBufferInfo envBufferInfo{};
        envBufferInfo.buffer = aCore.myEnvDataBuffer.myBuffer;
        envBufferInfo.offset = aCore.PadUniformBufferSize( sizeof( GpuEnvironmentData ) ) * i;
        envBufferInfo.range  = sizeof( GpuEnvironmentData );

        for ( uint32_t viewIndex = 0; viewIndex < kGfxMaxRenderViews; ++viewIndex ) {
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool     = aCore.mySceneGpuCtx.myDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts        = &aCore.mySceneGpuCtx.myGlobalSetLayout;
            if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &aCore.myFrameCtx.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "failed to allocate descriptor sets!" );
            }

            std::array< VkWriteDescriptorSet, 2 > descriptorWrites{};
            VkDescriptorBufferInfo                camBufferInfo{};
            camBufferInfo.buffer  = aCore.myFrameCtx.myFrameDatas[ i ].myCameraBuffer.myBuffer;
            camBufferInfo.offset  = aCore.PadUniformBufferSize( sizeof( GpuCameraData ) ) * viewIndex;
            camBufferInfo.range   = sizeof( GpuCameraData );
            descriptorWrites[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameCtx.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ],
                                                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &camBufferInfo, eVk_CameraBinding, 1 );
            descriptorWrites[ 1 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameCtx.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ],
                                                                          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &envBufferInfo, eVk_EnvBinding, 1 );
            vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
        }

        VkDescriptorSetAllocateInfo objectAllocInfo{};
        objectAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        objectAllocInfo.descriptorPool     = aCore.mySceneGpuCtx.myDescriptorPool;
        objectAllocInfo.descriptorSetCount = 1;
        objectAllocInfo.pSetLayouts        = &aCore.mySceneGpuCtx.myObjectSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &objectAllocInfo, &aCore.myFrameCtx.myFrameDatas[ i ].myObjectDescriptor ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate object descriptor sets!" );
        }

        VkDescriptorBufferInfo objectBufferInfo{};
        objectBufferInfo.buffer          = aCore.myFrameCtx.myFrameDatas[ i ].myObjectBuffer.myBuffer;
        objectBufferInfo.offset          = 0;
        objectBufferInfo.range           = sizeof( GpuObjectData );
        VkWriteDescriptorSet objectWrite = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameCtx.myFrameDatas[ i ].myObjectDescriptor,
                                                                                 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &objectBufferInfo, eVk_ObjectModelBinding, 1 );
        vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, 1, &objectWrite, 0, nullptr );
    }
}

void Vk_DescriptorSystem::CreateMaterialDescriptorSets( Vk_Core& aCore ) {
    const size_t materialCount = aCore.mySceneGpuCtx.myResourceTables.GetMaterialCount();
    aCore.mySceneGpuCtx.myMaterialDescriptorSets.assign( materialCount, VK_NULL_HANDLE );
    aCore.mySceneGpuCtx.myMaterialParamBuffers.resize( materialCount );

    std::vector< VkDescriptorSetLayout > layouts( materialCount, aCore.mySceneGpuCtx.myMaterialSetLayout );
    VkDescriptorSetAllocateInfo          allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = aCore.mySceneGpuCtx.myDescriptorPool;
    allocInfo.descriptorSetCount = static_cast< uint32_t >( materialCount );
    allocInfo.pSetLayouts        = layouts.data();
    if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, aCore.mySceneGpuCtx.myMaterialDescriptorSets.data() ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate material descriptor sets!" );
    }

    for ( size_t materialId = 0; materialId < materialCount; ++materialId ) {
        const uint32_t      textureId   = aCore.mySceneGpuCtx.myResourceTables.GetTextureIdForMaterial( static_cast< uint32_t >( materialId ) );
        const Gfx_Texture&  texture     = aCore.mySceneGpuCtx.myResourceTables.GetTexture( textureId );
        const Gfx_Material& material    = aCore.mySceneGpuCtx.myResourceTables.GetMaterial( static_cast< uint32_t >( materialId ) );
        Vk_AllocatedBuffer& paramBuffer = aCore.mySceneGpuCtx.myMaterialParamBuffers[ materialId ];
        aCore.CreateBuffer( sizeof( GpuMaterialParams ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, paramBuffer, true );

        const GpuMaterialParams params = Gfx_MaterialToGpuParams( material );
        void*                   mapped = nullptr;
        vmaMapMemory( aCore.myDeviceCtx.myAllocator, paramBuffer.myAllocation, &mapped );
        memcpy( mapped, &params, sizeof( params ) );
        vmaUnmapMemory( aCore.myDeviceCtx.myAllocator, paramBuffer.myAllocation );

        const VmaAllocator  allocator  = aCore.myDeviceCtx.myAllocator;
        const VkBuffer      buffer     = paramBuffer.myBuffer;
        const VmaAllocation allocation = paramBuffer.myAllocation;
        aCore.GetSceneDeletionQueue().pushFunction( [ allocator, buffer, allocation ]() { vmaDestroyBuffer( allocator, buffer, allocation ); } );

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = texture.ImageView();
        imageInfo.sampler     = aCore.mySceneGpuCtx.myTextureSampler;
        VkDescriptorBufferInfo paramInfo{};
        paramInfo.buffer = paramBuffer.myBuffer;
        paramInfo.offset = 0;
        paramInfo.range  = sizeof( GpuMaterialParams );
        std::array< VkWriteDescriptorSet, 2 > writes{};
        writes[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.mySceneGpuCtx.myMaterialDescriptorSets[ materialId ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo,
                                                            eVk_MaterialTextureBinding, 1 );
        writes[ 1 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.mySceneGpuCtx.myMaterialDescriptorSets[ materialId ], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramInfo,
                                                            eVk_MaterialAlphaBinding, 1 );
        vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
    }

    UtilLogger::Info( "DESCRIPTOR", "Material sets allocated: " + std::to_string( materialCount ) );
}

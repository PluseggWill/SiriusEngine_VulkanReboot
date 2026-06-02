#include "Vk_DescriptorSystem.h"

#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"
#include "Vk_ShaderEffectMeta.h"

#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Logger.h"

#include <array>

void Vk_DescriptorSystem::InitDeviceLayouts( Vk_Core& aCore ) {
    CreateDescriptorSetLayout( aCore );
    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ) {
        CreateBindlessMaterialSetLayout( aCore );
        CreateBindlessPipelineLayout( aCore );
        // S2 2d: SPIR-V/contract already checked at build; catch hand-written layout drift at runtime on bindless GPUs.
        VkShaderEffectMeta::VerifyLitBindlessReflectionContract();
    }
    LogLayoutContract( aCore );
}

void Vk_DescriptorSystem::LogLayoutContract( const Vk_Core& aCore ) {
    const char* materialPath = aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ? "bindless" : "batch";
    const char* set1Summary  = aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ? "set1=textureArray+materialSSBO (TriangleFrag_Lit_Bindless.frag)"
                                                                                       : "set1=texSampler+MaterialData UBO per materialId (TriangleFrag_Lit.frag)";
    UtilLogger::Info( "DESCRIPTOR", std::string( "layout contract: path=" ) + materialPath + " sets=0,1,2 (frame,material,object) "
                                        + "set0=camera+env (TriangleVertex/TriangleFrag_Lit) set2=GpuObjectData DYNAMIC (TriangleVertex) " + set1Summary
                                        + " rebuild=LoadScene/UnloadScene; swapchain=RefreshMaterialPipelines only" );
}

void Vk_DescriptorSystem::InitSceneDescriptors( Vk_Core& aCore ) {
    CreateTextureSampler( aCore );
    CreateDescriptorPool( aCore );
    CreateDescriptorSets( aCore );

    if ( aCore.myMaterialPath == Vk_RenderMaterialPath::Batch ) {
        CreateMaterialDescriptorSets( aCore );
    }
    else {
        CreateBindlessDescriptorResources( aCore );
    }
}

void Vk_DescriptorSystem::CreateDescriptorSetLayout( Vk_Core& aCore ) {
    // Lit batch Set 0/1/2 from reflection_lit.json + layout hash cache (S2 phase 2b).
    const LitBatchDescriptorSetLayouts layouts = VkShaderEffectMeta::AcquireLitBatchDescriptorSetLayouts( aCore.myDevice, aCore.myDeletionQueue );
    aCore.myGlobalSetLayout                    = layouts.myGlobalSetLayout;
    aCore.myMaterialSetLayout                  = layouts.myMaterialSetLayout;
    aCore.myObjectSetLayout                    = layouts.myObjectSetLayout;

    if ( UtilEngineConfig::GetDescriptorLayoutMismatchTest() ) {
        VkShaderEffectMeta::RunLitBatchLayoutMismatchValidationTest( aCore );
    }
}

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
    if ( vkCreateDescriptorSetLayout( aCore.myDevice, &layoutInfo, nullptr, &aCore.myBindlessMaterialSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create bindless material descriptor set layout!" );
    }

    const VkDevice              device         = aCore.myDevice;
    const VkDescriptorSetLayout bindlessLayout = aCore.myBindlessMaterialSetLayout;
    aCore.myDeletionQueue.pushFunction( [ device, bindlessLayout ]() { vkDestroyDescriptorSetLayout( device, bindlessLayout, nullptr ); } );
}

void Vk_DescriptorSystem::CreateBindlessPipelineLayout( Vk_Core& aCore ) {
    const std::array< VkDescriptorSetLayout, 3 > setLayouts = { aCore.myGlobalSetLayout, aCore.myBindlessMaterialSetLayout, aCore.myObjectSetLayout };
    VkPipelineLayoutCreateInfo                   layoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    layoutInfo.setLayoutCount                               = static_cast< uint32_t >( setLayouts.size() );
    layoutInfo.pSetLayouts                                  = setLayouts.data();
    if ( vkCreatePipelineLayout( aCore.myDevice, &layoutInfo, nullptr, &aCore.myBindlessPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create bindless pipeline layout!" );
    }

    const VkDevice         device                 = aCore.myDevice;
    const VkPipelineLayout bindlessPipelineLayout = aCore.myBindlessPipelineLayout;
    aCore.myDeletionQueue.pushFunction( [ device, bindlessPipelineLayout ]() { vkDestroyPipelineLayout( device, bindlessPipelineLayout, nullptr ); } );
}

void Vk_DescriptorSystem::CreateBindlessDescriptorResources( Vk_Core& aCore ) {
    const size_t textureCount  = aCore.myResourceTables.GetTextureCount();
    const size_t materialCount = aCore.myResourceTables.GetMaterialCount();

    std::vector< VkDescriptorImageInfo > imageInfos( VkDescriptorPolicy::kMaxBindlessTextures );
    for ( size_t textureId = 0; textureId < textureCount && textureId < VkDescriptorPolicy::kMaxBindlessTextures; ++textureId ) {
        const Gfx_Texture& texture          = aCore.myResourceTables.GetTexture( static_cast< uint32_t >( textureId ) );
        imageInfos[ textureId ].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[ textureId ].imageView   = texture.ImageView();
        imageInfos[ textureId ].sampler     = aCore.myTextureSampler;
    }

    std::vector< GpuMaterialTableEntry > tableEntries( materialCount );
    for ( size_t materialId = 0; materialId < materialCount; ++materialId ) {
        const uint32_t      textureId             = aCore.myResourceTables.GetTextureIdForMaterial( static_cast< uint32_t >( materialId ) );
        const Gfx_Material& material              = aCore.myResourceTables.GetMaterial( static_cast< uint32_t >( materialId ) );
        tableEntries[ materialId ].myTextureIndex    = textureId;
        tableEntries[ materialId ].myRoughness       = material.myRoughness;
        tableEntries[ materialId ].myMetallic        = material.myMetallic;
        tableEntries[ materialId ].myAlpha           = material.myAlpha;
        tableEntries[ materialId ].myAlphaMode = material.myAlphaMode;  // bindless frag mask discard
        tableEntries[ materialId ].myBaseColorFactor = material.myBaseColorFactor;
    }

    const VkDeviceSize tableSize = sizeof( GpuMaterialTableEntry ) * materialCount;
    aCore.CreateBuffer( tableSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, aCore.myMaterialTableBuffer, true );
    void* mapped = nullptr;
    vmaMapMemory( aCore.myAllocator, aCore.myMaterialTableBuffer.myAllocation, &mapped );
    memcpy( mapped, tableEntries.data(), static_cast< size_t >( tableSize ) );
    vmaUnmapMemory( aCore.myAllocator, aCore.myMaterialTableBuffer.myAllocation );

    const VmaAllocator  allocator       = aCore.myAllocator;
    const VkBuffer      tableBuffer     = aCore.myMaterialTableBuffer.myBuffer;
    const VmaAllocation tableAllocation = aCore.myMaterialTableBuffer.myAllocation;
    aCore.GetSceneDeletionQueue().pushFunction( [ allocator, tableBuffer, tableAllocation ]() { vmaDestroyBuffer( allocator, tableBuffer, tableAllocation ); } );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = aCore.myDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &aCore.myBindlessMaterialSetLayout;
    if ( vkAllocateDescriptorSets( aCore.myDevice, &allocInfo, &aCore.myBindlessDescriptorSet ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate bindless descriptor set!" );
    }

    VkDescriptorBufferInfo tableBufferInfo{};
    tableBufferInfo.buffer = aCore.myMaterialTableBuffer.myBuffer;
    tableBufferInfo.offset = 0;
    tableBufferInfo.range  = tableSize;

    std::array< VkWriteDescriptorSet, 2 > writes{};
    writes[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myBindlessDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos.data(),
                                                        eVk_BindlessTextureArrayBinding, static_cast< uint32_t >( VkDescriptorPolicy::kMaxBindlessTextures ) );
    writes[ 1 ] =
        VkInit::DescriptorSetWriteCreateInfo( aCore.myBindlessDescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tableBufferInfo, eVk_BindlessMaterialTableBinding, 1 );
    vkUpdateDescriptorSets( aCore.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );

    UtilLogger::Info( "BINDLESS", "bindless set ready: textures=" + std::to_string( textureCount ) + " materials=" + std::to_string( materialCount )
                                      + " tableGeneration=" + std::to_string( aCore.myResourceTables.GetMaterialTableGeneration() ) );
}

void Vk_DescriptorSystem::CreateDescriptorPool( Vk_Core& aCore ) {
    std::array< VkDescriptorPoolSize, 5 > poolSizes{};
    poolSizes[ 0 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[ 0 ].descriptorCount = 32;
    poolSizes[ 1 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSizes[ 1 ].descriptorCount = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );
    poolSizes[ 2 ].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[ 2 ].descriptorCount = aCore.myMaterialPath == Vk_RenderMaterialPath::Bindless ? VkDescriptorPolicy::kMaxBindlessTextures + 4 : 16;
    poolSizes[ 3 ].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[ 3 ].descriptorCount = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT );
    poolSizes[ 4 ].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[ 4 ].descriptorCount = 4;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast< uint32_t >( MAX_FRAMES_IN_FLIGHT ) * 2 + 16;
    if ( vkCreateDescriptorPool( aCore.myDevice, &poolInfo, nullptr, &aCore.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create descriptor pool!" );
    }

    const VkDevice         device = aCore.myDevice;
    const VkDescriptorPool pool   = aCore.myDescriptorPool;
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
    vkGetPhysicalDeviceProperties( aCore.myPhysicalDevice, &properties );
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod                  = static_cast< float >( aCore.myTextureImageMipLevels );
    if ( vkCreateSampler( aCore.myDevice, &samplerInfo, nullptr, &aCore.myTextureSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create texture sampler!" );
    }

    const VkDevice  device  = aCore.myDevice;
    const VkSampler sampler = aCore.myTextureSampler;
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
            allocInfo.descriptorPool     = aCore.myDescriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts        = &aCore.myGlobalSetLayout;
            if ( vkAllocateDescriptorSets( aCore.myDevice, &allocInfo, &aCore.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ] ) != VK_SUCCESS ) {
                throw std::runtime_error( "failed to allocate descriptor sets!" );
            }

            std::array< VkWriteDescriptorSet, 2 > descriptorWrites{};
            VkDescriptorBufferInfo                camBufferInfo{};
            camBufferInfo.buffer = aCore.myFrameDatas[ i ].myCameraBuffer.myBuffer;
            camBufferInfo.offset = aCore.PadUniformBufferSize( sizeof( GpuCameraData ) ) * viewIndex;
            camBufferInfo.range  = sizeof( GpuCameraData );
            descriptorWrites[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                          &camBufferInfo, eVk_CameraBinding, 1 );
            descriptorWrites[ 1 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameDatas[ i ].myGlobalDescriptors[ viewIndex ], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                          &envBufferInfo, eVk_EnvBinding, 1 );
            vkUpdateDescriptorSets( aCore.myDevice, static_cast< uint32_t >( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
        }

        VkDescriptorSetAllocateInfo objectAllocInfo{};
        objectAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        objectAllocInfo.descriptorPool     = aCore.myDescriptorPool;
        objectAllocInfo.descriptorSetCount = 1;
        objectAllocInfo.pSetLayouts        = &aCore.myObjectSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myDevice, &objectAllocInfo, &aCore.myFrameDatas[ i ].myObjectDescriptor ) != VK_SUCCESS ) {
            throw std::runtime_error( "failed to allocate object descriptor sets!" );
        }

        VkDescriptorBufferInfo objectBufferInfo{};
        objectBufferInfo.buffer          = aCore.myFrameDatas[ i ].myObjectBuffer.myBuffer;
        objectBufferInfo.offset          = 0;
        objectBufferInfo.range           = sizeof( GpuObjectData );
        VkWriteDescriptorSet objectWrite = VkInit::DescriptorSetWriteCreateInfo( aCore.myFrameDatas[ i ].myObjectDescriptor, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                                                                 &objectBufferInfo, eVk_ObjectModelBinding, 1 );
        vkUpdateDescriptorSets( aCore.myDevice, 1, &objectWrite, 0, nullptr );
    }
}

void Vk_DescriptorSystem::CreateMaterialDescriptorSets( Vk_Core& aCore ) {
    const size_t materialCount = aCore.myResourceTables.GetMaterialCount();
    aCore.myMaterialDescriptorSets.assign( materialCount, VK_NULL_HANDLE );
    aCore.myMaterialParamBuffers.resize( materialCount );

    std::vector< VkDescriptorSetLayout > layouts( materialCount, aCore.myMaterialSetLayout );
    VkDescriptorSetAllocateInfo          allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = aCore.myDescriptorPool;
    allocInfo.descriptorSetCount = static_cast< uint32_t >( materialCount );
    allocInfo.pSetLayouts        = layouts.data();
    if ( vkAllocateDescriptorSets( aCore.myDevice, &allocInfo, aCore.myMaterialDescriptorSets.data() ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to allocate material descriptor sets!" );
    }

    for ( size_t materialId = 0; materialId < materialCount; ++materialId ) {
        const uint32_t      textureId   = aCore.myResourceTables.GetTextureIdForMaterial( static_cast< uint32_t >( materialId ) );
        const Gfx_Texture&  texture     = aCore.myResourceTables.GetTexture( textureId );
        const Gfx_Material& material    = aCore.myResourceTables.GetMaterial( static_cast< uint32_t >( materialId ) );
        Vk_AllocatedBuffer& paramBuffer = aCore.myMaterialParamBuffers[ materialId ];
        aCore.CreateBuffer( sizeof( GpuMaterialParams ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, paramBuffer, true );

        const GpuMaterialParams params = Gfx_MaterialToGpuParams( material );
        void* mapped   = nullptr;
        vmaMapMemory( aCore.myAllocator, paramBuffer.myAllocation, &mapped );
        memcpy( mapped, &params, sizeof( params ) );
        vmaUnmapMemory( aCore.myAllocator, paramBuffer.myAllocation );

        const VmaAllocator  allocator  = aCore.myAllocator;
        const VkBuffer      buffer     = paramBuffer.myBuffer;
        const VmaAllocation allocation = paramBuffer.myAllocation;
        aCore.GetSceneDeletionQueue().pushFunction( [ allocator, buffer, allocation ]() { vmaDestroyBuffer( allocator, buffer, allocation ); } );

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = texture.ImageView();
        imageInfo.sampler     = aCore.myTextureSampler;
        VkDescriptorBufferInfo paramInfo{};
        paramInfo.buffer = paramBuffer.myBuffer;
        paramInfo.offset = 0;
        paramInfo.range  = sizeof( GpuMaterialParams );
        std::array< VkWriteDescriptorSet, 2 > writes{};
        writes[ 0 ] = VkInit::DescriptorSetWriteCreateInfo( aCore.myMaterialDescriptorSets[ materialId ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo,
                                                            eVk_MaterialTextureBinding, 1 );
        writes[ 1 ] =
            VkInit::DescriptorSetWriteCreateInfo( aCore.myMaterialDescriptorSets[ materialId ], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &paramInfo, eVk_MaterialAlphaBinding, 1 );
        vkUpdateDescriptorSets( aCore.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
    }

    UtilLogger::Info( "DESCRIPTOR", "Material sets allocated: " + std::to_string( materialCount ) );
}

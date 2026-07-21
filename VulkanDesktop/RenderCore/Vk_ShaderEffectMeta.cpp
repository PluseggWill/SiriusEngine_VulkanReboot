#include "Vk_ShaderEffectMeta.h"

#include "../Gfx/Gfx_RenderCamera.h"
#include "Vk_DataStruct.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_DeviceContext.h"
#include "Vk_Enum.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_SceneGpuContext.h"

#include "../Util/Util_DebugMessenger.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>

namespace {

using Json = nlohmann::json;

constexpr size_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr size_t kFnvPrime       = 1099511628211ull;

size_t HashCombine( size_t aHash, size_t aValue ) {
    return ( aHash ^ aValue ) * kFnvPrime;
}

VkShaderStageFlags StagesFromJson( const Json& aStages ) {
    VkShaderStageFlags flags = 0;
    for ( const Json& stage : aStages ) {
        const std::string label = stage.get< std::string >();
        if ( label == "VERTEX" ) {
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        else if ( label == "FRAGMENT" ) {
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }
    return flags;
}

VkDescriptorType DescriptorTypeFromName( const std::string& aName ) {
    if ( aName == "UNIFORM_BUFFER" ) {
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    if ( aName == "UNIFORM_BUFFER_DYNAMIC" ) {
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
    if ( aName == "COMBINED_IMAGE_SAMPLER" ) {
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    if ( aName == "STORAGE_BUFFER" ) {
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    throw std::runtime_error( "Vk_ShaderEffectMeta: unsupported descriptorType " + aName );
}

struct CachedLitBatchLayouts {
    size_t                myHash            = 0;
    VkDescriptorSetLayout mySetLayouts[ 3 ] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
};

// Process-lifetime cache: lit_batch layouts are created once per device boot (InitDeviceLayouts).
CachedLitBatchLayouts gLitBatchLayoutCache{};

// Sorted binding order required by Vulkan (ascending binding index).
std::vector< VkDescriptorSetLayoutBinding > BuildVkBindings( const DescriptorSetLayoutData& aSetData ) {
    std::vector< uint32_t > bindingOrder;
    bindingOrder.reserve( aSetData.myBindings.size() );
    for ( const auto& pair : aSetData.myBindings ) {
        bindingOrder.push_back( pair.first );
    }
    std::sort( bindingOrder.begin(), bindingOrder.end() );

    std::vector< VkDescriptorSetLayoutBinding > vkBindings;
    vkBindings.reserve( bindingOrder.size() );
    for ( uint32_t bindingIndex : bindingOrder ) {
        const ShaderResource&        resource = aSetData.myBindings.at( bindingIndex );
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding            = resource.myBinding;
        vkBinding.descriptorType     = resource.myType;
        vkBinding.descriptorCount    = resource.myCount;
        vkBinding.stageFlags         = resource.myStageFlags;
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back( vkBinding );
    }
    return vkBindings;
}

VkDescriptorSetLayout CreateOneSetLayout( VkDevice aDevice, const DescriptorSetLayoutData& aSetData ) {
    const std::vector< VkDescriptorSetLayoutBinding > vkBindings = BuildVkBindings( aSetData );
    VkDescriptorSetLayoutCreateInfo                   createInfo{};
    createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast< uint32_t >( vkBindings.size() );
    createInfo.pBindings    = vkBindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if ( vkCreateDescriptorSetLayout( aDevice, &createInfo, nullptr, &layout ) != VK_SUCCESS ) {
        throw std::runtime_error( "failed to create descriptor set layout for set " + std::to_string( aSetData.mySetNumber ) );
    }
    return layout;
}

}  // namespace

namespace VkShaderEffectMeta {

ShaderEffectMeta LoadLitBatchFromReflectionJson( const Util_EngineConfig& aConfig, const std::string& aLogicalPath ) {
    const std::string resolvedPath = UtilLoader::ResolvePath( aConfig, aLogicalPath );
    std::ifstream     file( resolvedPath );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_ShaderEffectMeta: cannot open reflection JSON: " + resolvedPath );
    }

    Json root;
    try {
        file >> root;
    }
    catch ( const Json::exception& e ) {
        throw std::runtime_error( std::string( "Vk_ShaderEffectMeta: invalid JSON: " ) + resolvedPath + " ?" + e.what() );
    }

    ShaderEffectMeta meta{};
    meta.myPipelineGroup = root.value( "pipelineGroup", "" );
    if ( !root.contains( "sets" ) || !root[ "sets" ].is_array() ) {
        throw std::runtime_error( "Vk_ShaderEffectMeta: reflection JSON missing sets[]" );
    }

    for ( const Json& setEntry : root[ "sets" ] ) {
        const uint32_t           setNumber = setEntry.at( "set" ).get< uint32_t >();
        DescriptorSetLayoutData& setData   = meta.mySets[ setNumber ];
        setData.mySetNumber                = setNumber;

        for ( const Json& bindingEntry : setEntry.at( "bindings" ) ) {
            ShaderResource resource{};
            resource.myBinding                       = bindingEntry.at( "binding" ).get< uint32_t >();
            resource.myName                          = bindingEntry.value( "name", "" );
            resource.myCount                         = bindingEntry.value( "count", 1 );
            resource.myType                          = DescriptorTypeFromName( bindingEntry.at( "descriptorType" ).get< std::string >() );
            resource.myStageFlags                    = StagesFromJson( bindingEntry.at( "stages" ) );
            setData.myBindings[ resource.myBinding ] = std::move( resource );
        }
    }

    return meta;
}

// Reuses lit_batch JSON parser; pipelineGroup must be lit_bindless (see DescriptorContract_LitBindless.json).
ShaderEffectMeta LoadLitBindlessFromReflectionJson( const Util_EngineConfig& aConfig, const std::string& aLogicalPath ) {
    ShaderEffectMeta meta = LoadLitBatchFromReflectionJson( aConfig, aLogicalPath );
    if ( meta.myPipelineGroup != "lit_bindless" ) {
        throw std::runtime_error( "Vk_ShaderEffectMeta: expected pipelineGroup lit_bindless, got " + meta.myPipelineGroup );
    }
    return meta;
}

void ApplyLitBatchLayoutOverrides( ShaderEffectMeta& aMeta ) {
    // CONTRACT: SPIR-V/JSON use UNIFORM_BUFFER for objectData; Vk layout uses DYNAMIC (VkDescriptorPolicy Set 2).
    auto setIt = aMeta.mySets.find( VkDescriptorPolicy::kSetObject );
    if ( setIt == aMeta.mySets.end() ) {
        return;
    }
    auto bindingIt = setIt->second.myBindings.find( eVk_ObjectModelBinding );
    if ( bindingIt == setIt->second.myBindings.end() ) {
        return;
    }
    if ( bindingIt->second.myType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
        bindingIt->second.myType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }
}

size_t HashLayout( const ShaderEffectMeta& aMeta ) {
    size_t hash = kFnvOffsetBasis;
    hash        = HashCombine( hash, std::hash< std::string >{}( aMeta.myPipelineGroup ) );

    std::vector< uint32_t > setNumbers;
    setNumbers.reserve( aMeta.mySets.size() );
    for ( const auto& setPair : aMeta.mySets ) {
        setNumbers.push_back( setPair.first );
    }
    std::sort( setNumbers.begin(), setNumbers.end() );

    for ( uint32_t setNumber : setNumbers ) {
        hash                                   = HashCombine( hash, setNumber );
        const DescriptorSetLayoutData& setData = aMeta.mySets.at( setNumber );

        std::vector< uint32_t > bindingNumbers;
        bindingNumbers.reserve( setData.myBindings.size() );
        for ( const auto& bindingPair : setData.myBindings ) {
            bindingNumbers.push_back( bindingPair.first );
        }
        std::sort( bindingNumbers.begin(), bindingNumbers.end() );

        for ( uint32_t bindingNumber : bindingNumbers ) {
            const ShaderResource& resource = setData.myBindings.at( bindingNumber );
            hash                           = HashCombine( hash, bindingNumber );
            hash                           = HashCombine( hash, static_cast< size_t >( resource.myType ) );
            hash                           = HashCombine( hash, resource.myCount );
            hash                           = HashCombine( hash, resource.myStageFlags );
        }
    }
    return hash;
}

void LogMetaDump( const ShaderEffectMeta& aMeta ) {
    UtilLogger::Info( "DESCRIPTOR", "ShaderEffectMeta pipelineGroup=" + aMeta.myPipelineGroup );

    std::vector< uint32_t > setNumbers;
    for ( const auto& setPair : aMeta.mySets ) {
        setNumbers.push_back( setPair.first );
    }
    std::sort( setNumbers.begin(), setNumbers.end() );

    for ( uint32_t setNumber : setNumbers ) {
        const DescriptorSetLayoutData& setData = aMeta.mySets.at( setNumber );
        std::vector< uint32_t >        bindingNumbers;
        for ( const auto& bindingPair : setData.myBindings ) {
            bindingNumbers.push_back( bindingPair.first );
        }
        std::sort( bindingNumbers.begin(), bindingNumbers.end() );

        for ( uint32_t bindingNumber : bindingNumbers ) {
            const ShaderResource& resource = setData.myBindings.at( bindingNumber );
            const char*           typeName = "UNKNOWN";
            switch ( resource.myType ) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                typeName = "UNIFORM_BUFFER";
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                typeName = "UNIFORM_BUFFER_DYNAMIC";
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                typeName = "COMBINED_IMAGE_SAMPLER";
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                typeName = "STORAGE_BUFFER";
                break;
            default:
                break;
            }
            const std::string stageLabel = ( resource.myStageFlags & VK_SHADER_STAGE_VERTEX_BIT ) != 0 && ( resource.myStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT ) != 0
                                               ? "VERTEX|FRAGMENT"
                                           : ( resource.myStageFlags & VK_SHADER_STAGE_VERTEX_BIT ) != 0   ? "VERTEX"
                                           : ( resource.myStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT ) != 0 ? "FRAGMENT"
                                                                                                           : "NONE";
            UtilLogger::Info( "DESCRIPTOR", "  set=" + std::to_string( setNumber ) + " binding=" + std::to_string( bindingNumber ) + " name=" + resource.myName
                                                + " type=" + typeName + " stages=" + stageLabel );
        }
    }
}

LitBatchDescriptorSetLayouts AcquireLitBatchDescriptorSetLayouts( Vk_Renderer& aCore ) {
    ShaderEffectMeta meta = LoadLitBatchFromReflectionJson( aCore.EngineConfig() );
    if ( meta.myPipelineGroup != "lit_batch" ) {
        throw std::runtime_error( "Vk_ShaderEffectMeta: expected pipelineGroup lit_batch, got " + meta.myPipelineGroup );
    }
    ApplyLitBatchLayoutOverrides( meta );
    LogMetaDump( meta );

    LitBatchDescriptorSetLayouts result{};
    const size_t                 layoutHash = HashLayout( meta );
    if ( gLitBatchLayoutCache.myHash == layoutHash && gLitBatchLayoutCache.mySetLayouts[ 0 ] != VK_NULL_HANDLE ) {
        UtilLogger::Info( "DESCRIPTOR", "layout cache hit hash=" + std::to_string( layoutHash ) );
        result.myGlobalSetLayout   = gLitBatchLayoutCache.mySetLayouts[ 0 ];
        result.myMaterialSetLayout = gLitBatchLayoutCache.mySetLayouts[ 1 ];
        result.myObjectSetLayout   = gLitBatchLayoutCache.mySetLayouts[ 2 ];
        return result;
    }

    UtilLogger::Info( "DESCRIPTOR", "layout cache miss hash=" + std::to_string( layoutHash ) );

    for ( uint32_t setIndex = 0; setIndex < 3; ++setIndex ) {
        if ( meta.mySets.find( setIndex ) == meta.mySets.end() ) {
            throw std::runtime_error( "Vk_ShaderEffectMeta: missing set " + std::to_string( setIndex ) + " in reflection meta" );
        }
    }

    const VkDevice        aDevice      = aCore.myRhi.myDeviceCtx.myDevice;
    VkDescriptorSetLayout layouts[ 3 ] = {};
    for ( uint32_t setIndex = 0; setIndex < 3; ++setIndex ) {
        layouts[ setIndex ] = CreateOneSetLayout( aDevice, meta.mySets.at( setIndex ) );
    }

    gLitBatchLayoutCache.myHash            = layoutHash;
    gLitBatchLayoutCache.mySetLayouts[ 0 ] = layouts[ 0 ];
    gLitBatchLayoutCache.mySetLayouts[ 1 ] = layouts[ 1 ];
    gLitBatchLayoutCache.mySetLayouts[ 2 ] = layouts[ 2 ];

    result.myGlobalSetLayout   = layouts[ 0 ];
    result.myMaterialSetLayout = layouts[ 1 ];
    result.myObjectSetLayout   = layouts[ 2 ];

    const VkDescriptorSetLayout globalLayout   = layouts[ 0 ];
    const VkDescriptorSetLayout materialLayout = layouts[ 1 ];
    const VkDescriptorSetLayout objectLayout   = layouts[ 2 ];
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ aDevice, globalLayout, materialLayout, objectLayout ]() {
        vkDestroyDescriptorSetLayout( aDevice, globalLayout, nullptr );
        vkDestroyDescriptorSetLayout( aDevice, materialLayout, nullptr );
        vkDestroyDescriptorSetLayout( aDevice, objectLayout, nullptr );
    } );
    return result;
}

// Runtime guard (2d): hand-written bindless Set 1 in Vk_DescriptorSystem must match reflection_lit_bindless.json + Vk_Enum.h.
namespace {

    const ShaderResource* FindBinding( const ShaderEffectMeta& aMeta, uint32_t aSet, uint32_t aBinding ) {
        const auto setIt = aMeta.mySets.find( aSet );
        if ( setIt == aMeta.mySets.end() ) {
            return nullptr;
        }
        const auto bindingIt = setIt->second.myBindings.find( aBinding );
        if ( bindingIt == setIt->second.myBindings.end() ) {
            return nullptr;
        }
        return &bindingIt->second;
    }

    void ExpectBinding( const ShaderResource* aResource, uint32_t aSet, uint32_t aBinding, VkDescriptorType aType, VkShaderStageFlags aStages, const char* aContext ) {
        if ( aResource == nullptr ) {
            throw std::runtime_error( std::string( "Vk_ShaderEffectMeta bindless verify: missing set " ) + std::to_string( aSet ) + " binding " + std::to_string( aBinding )
                                      + " (" + aContext + ")" );
        }
        if ( aResource->myType != aType || ( aResource->myStageFlags & aStages ) != aStages ) {
            throw std::runtime_error( std::string( "Vk_ShaderEffectMeta bindless verify: " ) + aContext + " type/stage mismatch" );
        }
    }

}  // namespace

void VerifyLitBindlessReflectionContract( const Util_EngineConfig& aConfig ) {
    const ShaderEffectMeta meta = LoadLitBindlessFromReflectionJson( aConfig );

    const ShaderResource* textureArray  = FindBinding( meta, eVk_SetMaterial, eVk_BindlessTextureArrayBinding );
    const ShaderResource* materialTable = FindBinding( meta, eVk_SetMaterial, eVk_BindlessMaterialTableBinding );
    ExpectBinding( textureArray, eVk_SetMaterial, eVk_BindlessTextureArrayBinding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, "u_Textures" );
    ExpectBinding( materialTable, eVk_SetMaterial, eVk_BindlessMaterialTableBinding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, "MaterialTable SSBO" );

    if ( textureArray->myCount != VkDescriptorPolicy::kMaxBindlessTextures ) {
        throw std::runtime_error( "Vk_ShaderEffectMeta bindless verify: u_Textures SPIR-V count " + std::to_string( textureArray->myCount )
                                  + " != engine kMaxBindlessTextures " + std::to_string( VkDescriptorPolicy::kMaxBindlessTextures )
                                  + " (rebuild shaders / sync TriangleFrag_Lit_Bindless.frag)" );
    }

    UtilLogger::Info( "DESCRIPTOR", "bindless reflection verify OK: set1 binding0 name=" + textureArray->myName + " descriptorCount=" + std::to_string( textureArray->myCount )
                                        + "; binding1 name=" + materialTable->myName + " STORAGE_BUFFER" );
}

}  // namespace VkShaderEffectMeta

// Dev-only layout mismatch probe; takes narrow contexts instead of Vk_Renderer friend (phase 4).
void VkShaderEffectMeta_RunLitBatchLayoutMismatchValidationTest( Vk_DeviceContext& aDevice, Vk_SceneGpuContext& aScene, Vk_Renderer& aCoreOps ) {
    ( void )aScene;
    if ( !aCoreOps.RenderFeatures().myValidationEnabled ) {
        throw std::runtime_error( "--descriptor-layout-mismatch-test requires validation layers (use --validation, not --no-validation)" );
    }
    if ( !UtilDebugMessenger::HasActiveMessenger() ) {
        throw std::runtime_error( "layout mismatch test: VK_LAYER_KHRONOS_validation not available (install Vulkan SDK; see Docs/validation-layers.md)" );
    }

    UtilLogger::Info( "DESCRIPTOR", "layout mismatch test: creating wrong Set 2 layout (COMBINED_IMAGE_SAMPLER vs reflected DYNAMIC UBO)" );

    VkDescriptorSetLayoutBinding wrongBinding{};
    wrongBinding.binding            = eVk_ObjectModelBinding;
    wrongBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wrongBinding.descriptorCount    = 1;
    wrongBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
    wrongBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo wrongLayoutInfo{};
    wrongLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    wrongLayoutInfo.bindingCount = 1;
    wrongLayoutInfo.pBindings    = &wrongBinding;

    VkDescriptorSetLayout wrongLayout = VK_NULL_HANDLE;
    if ( vkCreateDescriptorSetLayout( aDevice.myDevice, &wrongLayoutInfo, nullptr, &wrongLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "layout mismatch test: failed to create wrong descriptor set layout" );
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 1;

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if ( vkCreateDescriptorPool( aDevice.myDevice, &poolInfo, nullptr, &pool ) != VK_SUCCESS ) {
        vkDestroyDescriptorSetLayout( aDevice.myDevice, wrongLayout, nullptr );
        throw std::runtime_error( "layout mismatch test: failed to create descriptor pool" );
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &wrongLayout;

    VkDescriptorSet wrongSet = VK_NULL_HANDLE;
    if ( vkAllocateDescriptorSets( aDevice.myDevice, &allocInfo, &wrongSet ) != VK_SUCCESS ) {
        vkDestroyDescriptorPool( aDevice.myDevice, pool, nullptr );
        vkDestroyDescriptorSetLayout( aDevice.myDevice, wrongLayout, nullptr );
        throw std::runtime_error( "layout mismatch test: failed to allocate descriptor set" );
    }

    Vk_AllocatedBuffer probeBuffer{};
    aCoreOps.CreateBuffer( sizeof( Gpu_ObjectData ), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, probeBuffer, true );

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = probeBuffer.myBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof( Gpu_ObjectData );

    UtilDebugMessenger::BeginValidationErrorCapture();
    VkWriteDescriptorSet mismatchWrite = VkInit::DescriptorSetWriteCreateInfo( wrongSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &bufferInfo, eVk_ObjectModelBinding, 1 );
    vkUpdateDescriptorSets( aDevice.myDevice, 1, &mismatchWrite, 0, nullptr );
    UtilDebugMessenger::EndValidationErrorCapture();

    std::string validationMessage;
    const bool  sawValidationError      = UtilDebugMessenger::TryConsumeCapturedValidationError( validationMessage );
    const bool  looksLikeDescriptorVuid = validationMessage.find( "VUID" ) != std::string::npos || validationMessage.find( "Descriptor" ) != std::string::npos
                                         || validationMessage.find( "descriptor" ) != std::string::npos;

    vmaDestroyBuffer( aDevice.myAllocator, probeBuffer.myBuffer, probeBuffer.myAllocation );
    vkDestroyDescriptorPool( aDevice.myDevice, pool, nullptr );
    vkDestroyDescriptorSetLayout( aDevice.myDevice, wrongLayout, nullptr );

    if ( !sawValidationError || !looksLikeDescriptorVuid ) {
        throw std::runtime_error( "layout mismatch test: expected validation ERROR for descriptor type mismatch; captured=" + validationMessage );
    }

    const size_t logLen = std::min( validationMessage.size(), static_cast< size_t >( 240 ) );
    UtilLogger::Info( "DESCRIPTOR", "layout mismatch test OK (validation caught): " + validationMessage.substr( 0, logLen ) );
}

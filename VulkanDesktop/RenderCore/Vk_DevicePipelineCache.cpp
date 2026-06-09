// Module: Vk_DevicePipelineCache ??load/save VkPipelineCache blob for faster pipeline creation on restart.
// Disk format: PipelineDiskCacheHeaderV1 + vkGetPipelineCacheData bytes (see anonymous helpers below).
#include "Vk_DevicePipelineCache.h"

#include "Vk_Bindless.h"
#include "Vk_Core.h"

#include "../Gfx/Gfx_ShaderPermutation.h"
#include "../Util/Util_EngineConfig.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "../Util/Util_VulkanResult.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr uint32_t kMagic   = 0x31435053u;  // 'SPC1' ??Sirius pipeline cache v1
constexpr uint32_t kVersion = 1u;

// CONTRACT: entire header must match at load time; any GPU/driver/shader change invalidates the blob.
#pragma pack( push, 1 )
struct PipelineDiskCacheHeaderV1 {
    uint32_t myMagic   = kMagic;
    uint32_t myVersion = kVersion;
    uint8_t  myPipelineCacheUuid[ VK_UUID_SIZE ]{};
    uint32_t myVendorId            = 0;
    uint32_t myDeviceId            = 0;
    uint32_t myDriverVersion       = 0;
    uint32_t myDeviceType          = 0;
    uint32_t myApiVersion          = 0;
    uint32_t myShaderFingerprint   = 0;
    uint32_t myActivePermutationId = 0;
    uint32_t myIncludeBindlessFrag = 0;
    uint32_t myReserved            = 0;
};
#pragma pack( pop )

uint32_t Fnv1a32( uint32_t aHash, const void* aData, size_t aSize ) {
    const uint8_t* bytes = static_cast< const uint8_t* >( aData );
    for ( size_t i = 0; i < aSize; ++i ) {
        aHash ^= bytes[ i ];
        aHash *= 16777619u;
    }
    return aHash;
}

uint32_t Fnv1a32String( uint32_t aHash, const std::string& aText ) {
    return Fnv1a32( aHash, aText.data(), aText.size() );
}

uint32_t HashFileMtime( uint32_t aHash, const std::filesystem::path& aPath ) {
    if ( !std::filesystem::exists( aPath ) ) {
        return Fnv1a32String( aHash, aPath.string() + ":missing" );
    }
    const auto time = std::filesystem::last_write_time( aPath );
    const auto ns   = std::chrono::duration_cast< std::chrono::nanoseconds >( time.time_since_epoch() ).count();
    aHash           = Fnv1a32String( aHash, aPath.string() );
    return Fnv1a32( aHash, &ns, sizeof( ns ) );
}

// FNV-1a over PermutationRegistry.json + active perm SPIR-V mtimes (bindless frag when bindless path).
uint32_t ComputeShaderFingerprint( const Util_EngineConfig& aConfig, bool aIncludeBindlessFrag ) {
    uint32_t hash = 2166136261u;

    hash = HashFileMtime( hash, UtilLoader::ResolvePath( aConfig, Gfx_ShaderPermutation::kRegistryLogicalPath ) );

    const Gfx_ShaderPermutationDef& active = Gfx_ShaderPermutation::GetActiveDefinition();
    hash                                   = HashFileMtime( hash, UtilLoader::ResolvePath( aConfig, active.myVertSpvLogicalPath ) );
    hash                                   = HashFileMtime( hash, UtilLoader::ResolvePath( aConfig, active.myFragSpvLogicalPath ) );

    if ( aIncludeBindlessFrag ) {
        hash = HashFileMtime( hash, UtilLoader::ResolvePath( aConfig, "VulkanDesktop/Shader_Generated/TrianglePix_Bindless.spv" ) );
    }

    return hash;
}

PipelineDiskCacheHeaderV1 BuildHeader( const VkPhysicalDeviceProperties& aProps, uint32_t aShaderFingerprint, bool aIncludeBindlessFrag ) {
    PipelineDiskCacheHeaderV1 header{};
    std::memcpy( header.myPipelineCacheUuid, aProps.pipelineCacheUUID, VK_UUID_SIZE );
    header.myVendorId            = aProps.vendorID;
    header.myDeviceId            = aProps.deviceID;
    header.myDriverVersion       = aProps.driverVersion;
    header.myDeviceType          = static_cast< uint32_t >( aProps.deviceType );
    header.myApiVersion          = aProps.apiVersion;
    header.myShaderFingerprint   = aShaderFingerprint;
    header.myActivePermutationId = Gfx_ShaderPermutation::GetActiveId();
    header.myIncludeBindlessFrag = aIncludeBindlessFrag ? 1u : 0u;
    return header;
}

bool HeaderMatchesCurrent( const PipelineDiskCacheHeaderV1& aDisk, const PipelineDiskCacheHeaderV1& aCurrent ) {
    return std::memcmp( &aDisk, &aCurrent, sizeof( PipelineDiskCacheHeaderV1 ) ) == 0;
}

std::filesystem::path DiskCachePath( const Util_EngineConfig& aConfig ) {
    return aConfig.GetAssetRoot() / "Cache" / "pipeline_cache_v1.bin";
}

bool TryLoadBlob( const Util_EngineConfig& aConfig, const PipelineDiskCacheHeaderV1& aExpectedHeader, std::vector< uint8_t >& aOutBlob ) {
    const std::filesystem::path path = DiskCachePath( aConfig );
    std::ifstream               file( path, std::ios::binary );
    if ( !file.is_open() ) {
        UtilLogger::Info( "PIPELINE-CACHE", "disk miss (no file): " + path.string() );
        return false;
    }

    PipelineDiskCacheHeaderV1 diskHeader{};
    file.read( reinterpret_cast< char* >( &diskHeader ), sizeof( diskHeader ) );
    if ( !file || diskHeader.myMagic != kMagic || diskHeader.myVersion != kVersion ) {
        UtilLogger::Info( "PIPELINE-CACHE", "disk miss (bad magic/version): " + path.string() );
        return false;
    }

    if ( !HeaderMatchesCurrent( diskHeader, aExpectedHeader ) ) {
        UtilLogger::Info( "PIPELINE-CACHE", "disk miss (fingerprint changed): shaderFp=" + std::to_string( diskHeader.myShaderFingerprint ) + " -> "
                                                + std::to_string( aExpectedHeader.myShaderFingerprint ) );
        return false;
    }

    file.seekg( 0, std::ios::end );
    const std::streamoff fileSize = file.tellg();
    const std::streamoff blobSize = fileSize - static_cast< std::streamoff >( sizeof( PipelineDiskCacheHeaderV1 ) );
    if ( blobSize <= 0 ) {
        UtilLogger::Info( "PIPELINE-CACHE", "disk miss (empty blob): " + path.string() );
        return false;
    }

    aOutBlob.resize( static_cast< size_t >( blobSize ) );
    file.seekg( sizeof( PipelineDiskCacheHeaderV1 ), std::ios::beg );
    file.read( reinterpret_cast< char* >( aOutBlob.data() ), blobSize );
    if ( !file ) {
        UtilLogger::Info( "PIPELINE-CACHE", "disk miss (truncated read): " + path.string() );
        aOutBlob.clear();
        return false;
    }

    UtilLogger::Info( "PIPELINE-CACHE", "disk hit: " + path.string() + " bytes=" + std::to_string( aOutBlob.size() ) );
    return true;
}

void SaveBlob( const Util_EngineConfig& aConfig, VkDevice aDevice, VkPipelineCache aCache, const PipelineDiskCacheHeaderV1& aHeader ) {
    if ( aCache == VK_NULL_HANDLE ) {
        return;
    }

    size_t   dataSize   = 0;
    VkResult sizeResult = vkGetPipelineCacheData( aDevice, aCache, &dataSize, nullptr );
    if ( sizeResult != VK_SUCCESS || dataSize == 0 ) {
        UtilLogger::Info( "PIPELINE-CACHE", "skip save (empty in-memory cache)." );
        return;
    }

    std::vector< uint8_t > blob( dataSize );
    VkResult               dataResult = vkGetPipelineCacheData( aDevice, aCache, &dataSize, blob.data() );
    if ( dataResult != VK_SUCCESS ) {
        UtilLogger::Warn( "PIPELINE-CACHE", "vkGetPipelineCacheData failed; skip save." );
        return;
    }
    blob.resize( dataSize );

    const std::filesystem::path path = DiskCachePath( aConfig );
    std::error_code             ec;
    std::filesystem::create_directories( path.parent_path(), ec );

    std::ofstream out( path, std::ios::binary | std::ios::trunc );
    if ( !out.is_open() ) {
        UtilLogger::Warn( "PIPELINE-CACHE", "could not open for write: " + path.string() );
        return;
    }

    out.write( reinterpret_cast< const char* >( &aHeader ), sizeof( aHeader ) );
    out.write( reinterpret_cast< const char* >( blob.data() ), static_cast< std::streamsize >( blob.size() ) );
    if ( !out ) {
        UtilLogger::Warn( "PIPELINE-CACHE", "write failed: " + path.string() );
        return;
    }

    UtilLogger::Info( "PIPELINE-CACHE", "saved " + path.string() + " blobBytes=" + std::to_string( blob.size() ) );
}

void CreateEmptyCache( VkDevice aDevice, VkPipelineCache& aOutCache ) {
    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    UtilVulkanResult::ThrowOnFailure( vkCreatePipelineCache( aDevice, &createInfo, nullptr, &aOutCache ), "vkCreatePipelineCache" );
    UtilLogger::Info( "PIPELINE-CACHE", "created empty in-memory cache." );
}

}  // namespace

void Vk_DevicePipelineCache::Create( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myPipelineCache != VK_NULL_HANDLE ) {
        return;
    }

    const bool                      includeBindless = ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless );
    const Util_EngineConfig&        cfg             = aCore.EngineConfig();
    const uint32_t                  shaderFp        = ComputeShaderFingerprint( cfg, includeBindless );
    const PipelineDiskCacheHeaderV1 header          = BuildHeader( aCore.myDeviceCtx.myPhysicalDeviceProperties, shaderFp, includeBindless );

    std::vector< uint8_t > initialBlob;
    const bool             loaded = TryLoadBlob( cfg, header, initialBlob );

    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if ( loaded && !initialBlob.empty() ) {
        createInfo.initialDataSize = initialBlob.size();
        createInfo.pInitialData    = initialBlob.data();
    }

    const VkResult result = vkCreatePipelineCache( aCore.myDeviceCtx.myDevice, &createInfo, nullptr, &aCore.myDeviceCtx.myPipelineCache );
    if ( result == VK_SUCCESS ) {
        UtilLogger::Info( "PIPELINE-CACHE", std::string( loaded ? "loaded" : "created" ) + " VkPipelineCache shaderFp=" + std::to_string( shaderFp )
                                                + " permId=" + std::to_string( header.myActivePermutationId ) );
        return;
    }

    // Driver may reject stale blobs after driver update even when our header matched.
    if ( loaded && ( result == VK_ERROR_INITIALIZATION_FAILED || result == VK_ERROR_INCOMPATIBLE_DRIVER ) ) {
        UtilLogger::Info( "PIPELINE-CACHE", "driver rejected disk blob; recreating empty cache." );
        CreateEmptyCache( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache );
        return;
    }

    UtilVulkanResult::ThrowOnFailure( result, "vkCreatePipelineCache" );
}

void Vk_DevicePipelineCache::Destroy( Vk_Core& aCore ) {
    if ( aCore.myDeviceCtx.myPipelineCache == VK_NULL_HANDLE || aCore.myDeviceCtx.myDevice == VK_NULL_HANDLE ) {
        return;
    }

    const bool                      includeBindless = ( aCore.myDeviceCtx.myMaterialPath == Vk_RenderMaterialPath::Bindless );
    const Util_EngineConfig&        cfg             = aCore.EngineConfig();
    const PipelineDiskCacheHeaderV1 header = BuildHeader( aCore.myDeviceCtx.myPhysicalDeviceProperties, ComputeShaderFingerprint( cfg, includeBindless ), includeBindless );
    SaveBlob( cfg, aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, header );

    vkDestroyPipelineCache( aCore.myDeviceCtx.myDevice, aCore.myDeviceCtx.myPipelineCache, nullptr );
    aCore.myDeviceCtx.myPipelineCache = VK_NULL_HANDLE;
}

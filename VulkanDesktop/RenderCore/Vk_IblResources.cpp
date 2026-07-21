// Module: Vk_IblResources — offline IBL cubemaps + BRDF LUT upload (S5).
#include "Vk_IblResources.h"

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_TextureLoader.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace {

constexpr VkFormat kSkyCubemapFormat    = VK_FORMAT_R8G8B8A8_SRGB;
constexpr VkFormat kLinearCubemapFormat = VK_FORMAT_R8G8B8A8_UNORM;  // irradiance + prefilter baked in linear
constexpr VkFormat kBrdfLutFormat       = VK_FORMAT_R8G8B8A8_UNORM;

struct PrefilterManifest {
    std::string myDir;
    uint32_t    myMipCount    = 1u;
    bool        myPerMipFaces = false;
};

struct EnvironmentManifest {
    std::string       myIrradianceDir;
    PrefilterManifest myPrefilter;
    std::string       mySkyDir;
    std::string       myBrdfLutPath;
};

PrefilterManifest ParsePrefilterEntry( const std::string& aEnvironmentRoot, const nlohmann::json& aPrefilter ) {
    PrefilterManifest prefilter{};
    if ( aPrefilter.is_string() ) {
        prefilter.myDir         = aEnvironmentRoot + "/" + aPrefilter.get< std::string >();
        prefilter.myMipCount    = 1u;
        prefilter.myPerMipFaces = false;
        return prefilter;
    }

    const std::string baseDir = aPrefilter.at( "baseDir" ).get< std::string >();
    prefilter.myDir           = aEnvironmentRoot + "/" + baseDir;
    prefilter.myMipCount      = aPrefilter.value( "mipCount", 1u );
    const std::string layout  = aPrefilter.value( "layout", std::string( "per-mip-faces" ) );
    if ( layout != "per-mip-faces" ) {
        throw std::runtime_error( "Vk_IblResources: unsupported prefilter layout: " + layout );
    }
    prefilter.myPerMipFaces = true;
    if ( prefilter.myMipCount == 0u ) {
        throw std::runtime_error( "Vk_IblResources: prefilter mipCount must be >= 1" );
    }
    return prefilter;
}

EnvironmentManifest LoadManifest( const Util_EngineConfig& aConfig, const std::string& aEnvironmentRoot ) {
    const std::string manifestPath = UtilLoader::ResolvePath( aConfig, aEnvironmentRoot + "/manifest.json" );
    std::ifstream     file( manifestPath );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_IblResources: missing manifest: " + manifestPath );
    }

    nlohmann::json root;
    file >> root;

    EnvironmentManifest manifest{};
    manifest.myIrradianceDir = aEnvironmentRoot + "/" + root.at( "irradiance" ).get< std::string >();
    manifest.myPrefilter     = ParsePrefilterEntry( aEnvironmentRoot, root.at( "prefilter" ) );
    manifest.mySkyDir        = aEnvironmentRoot + "/" + root.at( "sky" ).get< std::string >();
    manifest.myBrdfLutPath   = aEnvironmentRoot + "/" + root.at( "brdfLut" ).get< std::string >();
    return manifest;
}

void RegisterTextureDeletion( Vk_Renderer& aCore, Vk_TextureResource& aTexture ) {
    const VkDevice      device     = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator  allocator  = aCore.myRhi.myDeviceCtx.myAllocator;
    const VkImage       image      = aTexture.Image();
    const VmaAllocation allocation = aTexture.Allocation();
    const VkImageView   view       = aTexture.ImageView();
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, allocator, image, allocation, view ]() {
        if ( view != VK_NULL_HANDLE ) {
            vkDestroyImageView( device, view, nullptr );
        }
        if ( image != VK_NULL_HANDLE ) {
            vmaDestroyImage( allocator, image, allocation );
        }
    } );
}

VkSampler CreateCubemapSampler( VkDevice aDevice ) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.maxAnisotropy           = 1.0f;
    samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE;

    VkSampler sampler = VK_NULL_HANDLE;
    if ( vkCreateSampler( aDevice, &samplerInfo, nullptr, &sampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_IblResources: failed to create cubemap sampler" );
    }
    return sampler;
}

VkSampler CreateBrdfLutSampler( VkDevice aDevice ) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod                  = 1.0f;

    VkSampler sampler = VK_NULL_HANDLE;
    if ( vkCreateSampler( aDevice, &samplerInfo, nullptr, &sampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_IblResources: failed to create BRDF LUT sampler" );
    }
    return sampler;
}

void LoadPrefilterCubemap( const Util_EngineConfig& aConfig, const PrefilterManifest& aPrefilter, const Vk_ResourceContext& aContext, Vk_TextureResource& aOutTexture,
                           float& aOutMaxMipLevel ) {
    if ( aPrefilter.myPerMipFaces ) {
        Vk_TextureLoader::LoadCubemapMipChainFromFaceDirectories( aConfig, aPrefilter.myDir, aContext, kLinearCubemapFormat, aOutTexture, aPrefilter.myMipCount );
        aOutMaxMipLevel = static_cast< float >( aPrefilter.myMipCount - 1u );
        return;
    }

    Vk_TextureLoader::LoadCubemapFromFaceDirectory( aConfig, aPrefilter.myDir, aContext, kLinearCubemapFormat, aOutTexture, 1 );
    aOutMaxMipLevel = 0.0f;
}

}  // namespace

namespace Vk_IblResources {

void Destroy( Vk_Renderer& aCore ) {
    Vk_IblResourcesState& state = aCore.myIblResourcesState;
    if ( !state.myInitialized ) {
        return;
    }

    state.myInitialized = false;
}

void Init( Vk_Renderer& aCore, const std::string& aEnvironmentLogicalPath ) {
    if ( aCore.myIblResourcesState.myInitialized ) {
        return;
    }

    const EnvironmentManifest manifest = LoadManifest( aCore.EngineConfig(), aEnvironmentLogicalPath );

    Vk_TextureLoader::LoadCubemapFromFaceDirectory( aCore.EngineConfig(), manifest.myIrradianceDir, aCore.GetResourceContext(), kLinearCubemapFormat,
                                                    aCore.myIblResourcesState.myIrradiance, 1 );
    LoadPrefilterCubemap( aCore.EngineConfig(), manifest.myPrefilter, aCore.GetResourceContext(), aCore.myIblResourcesState.myPrefilter,
                          aCore.myIblResourcesState.myPrefilterMaxMipLevel );
    Vk_TextureLoader::LoadCubemapFromFaceDirectory( aCore.EngineConfig(), manifest.mySkyDir, aCore.GetResourceContext(), kSkyCubemapFormat, aCore.myIblResourcesState.mySky,
                                                    1 );
    Vk_TextureLoader::LoadImage2D( aCore.EngineConfig(), manifest.myBrdfLutPath, aCore.GetResourceContext(), kBrdfLutFormat, aCore.myIblResourcesState.myBrdfLut );

    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myIrradiance );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myPrefilter );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.mySky );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myBrdfLut );

    const VkDevice device                      = aCore.myRhi.myDeviceCtx.myDevice;
    aCore.myIblResourcesState.myCubemapSampler = CreateCubemapSampler( device );
    aCore.myIblResourcesState.myBrdfLutSampler = CreateBrdfLutSampler( device );

    const VkSampler cubemapSampler = aCore.myIblResourcesState.myCubemapSampler;
    const VkSampler brdfSampler    = aCore.myIblResourcesState.myBrdfLutSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, cubemapSampler, brdfSampler ]() {
        if ( cubemapSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, cubemapSampler, nullptr );
        }
        if ( brdfSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, brdfSampler, nullptr );
        }
    } );

    aCore.myIblResourcesState.myInitialized = true;
    UtilLogger::Info( "IBL", "Loaded environment '" + aEnvironmentLogicalPath + "' prefilterMaxMip=" + std::to_string( aCore.myIblResourcesState.myPrefilterMaxMipLevel ) );
}

}  // namespace Vk_IblResources

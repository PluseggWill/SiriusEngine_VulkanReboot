// Module: Vk_IblResources — offline IBL cubemaps + BRDF LUT upload (S5).
#include "Vk_IblResources.h"

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_Initializer.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace {

constexpr VkFormat kCubemapFormat = VK_FORMAT_R8G8B8A8_SRGB;
constexpr VkFormat kBrdfLutFormat = VK_FORMAT_R8G8B8A8_UNORM;

struct EnvironmentManifest {
    std::string myIrradianceDir;
    std::string myPrefilterDir;
    std::string mySkyDir;
    std::string myBrdfLutPath;
};

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
    manifest.myPrefilterDir  = aEnvironmentRoot + "/" + root.at( "prefilter" ).get< std::string >();
    manifest.mySkyDir        = aEnvironmentRoot + "/" + root.at( "sky" ).get< std::string >();
    manifest.myBrdfLutPath   = aEnvironmentRoot + "/" + root.at( "brdfLut" ).get< std::string >();
    return manifest;
}

void RegisterTextureDeletion( Vk_Core& aCore, Gfx_Texture& aTexture ) {
    const VkDevice      device     = aCore.myDeviceCtx.myDevice;
    const VmaAllocator  allocator  = aCore.myDeviceCtx.myAllocator;
    const VkImage       image      = aTexture.Image();
    const VmaAllocation allocation = aTexture.Allocation();
    const VkImageView   view       = aTexture.ImageView();
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, allocator, image, allocation, view ]() {
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

}  // namespace

namespace Vk_IblResources {

void Destroy( Vk_Core& aCore ) {
    Vk_IblResourcesState& state = aCore.myIblResourcesState;
    if ( !state.myInitialized ) {
        return;
    }

    state.myInitialized = false;
}

void Init( Vk_Core& aCore, const std::string& aEnvironmentLogicalPath ) {
    if ( aCore.myIblResourcesState.myInitialized ) {
        return;
    }

    const EnvironmentManifest manifest = LoadManifest( aCore.EngineConfig(), aEnvironmentLogicalPath );

    UtilLoader::LoadCubemapFromFaceDirectory( aCore.EngineConfig(), manifest.myIrradianceDir, aCore.myResourceContext, kCubemapFormat, aCore.myIblResourcesState.myIrradiance, 1 );
    UtilLoader::LoadCubemapFromFaceDirectory( aCore.EngineConfig(), manifest.myPrefilterDir, aCore.myResourceContext, kCubemapFormat, aCore.myIblResourcesState.myPrefilter, 1 );
    UtilLoader::LoadCubemapFromFaceDirectory( aCore.EngineConfig(), manifest.mySkyDir, aCore.myResourceContext, kCubemapFormat, aCore.myIblResourcesState.mySky, 1 );
    UtilLoader::LoadImage2D( aCore.EngineConfig(), manifest.myBrdfLutPath, aCore.myResourceContext, kBrdfLutFormat, aCore.myIblResourcesState.myBrdfLut );

    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myIrradiance );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myPrefilter );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.mySky );
    RegisterTextureDeletion( aCore, aCore.myIblResourcesState.myBrdfLut );

    const VkDevice device = aCore.myDeviceCtx.myDevice;
    aCore.myIblResourcesState.myCubemapSampler = CreateCubemapSampler( device );
    aCore.myIblResourcesState.myBrdfLutSampler  = CreateBrdfLutSampler( device );

    const VkSampler cubemapSampler = aCore.myIblResourcesState.myCubemapSampler;
    const VkSampler brdfSampler    = aCore.myIblResourcesState.myBrdfLutSampler;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, cubemapSampler, brdfSampler ]() {
        if ( cubemapSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, cubemapSampler, nullptr );
        }
        if ( brdfSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, brdfSampler, nullptr );
        }
    } );

    aCore.myIblResourcesState.myInitialized = true;
    UtilLogger::Info( "IBL", "Loaded environment '" + aEnvironmentLogicalPath + "' from manifest" );
}

}  // namespace Vk_IblResources

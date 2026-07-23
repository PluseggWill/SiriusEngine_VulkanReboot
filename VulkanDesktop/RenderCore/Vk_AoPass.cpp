// Module: Vk_AoPass — pluggable screen-space AO (Classic SSAO, HBAO+, GTAO).
// Outputs linear R8 myAoRaw; ShadowAoSoft and deferred read via GetRawAoImageView().
#include "Vk_AoPass.h"

#include "Vk_AoPassInternal.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoPass.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_FrameCmd.h"
#include "Vk_Initializer.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_AoPass::kMaxFramesInFlight ), "Gfx_AoPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace {

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_AoPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

constexpr char kClassicShaderPath[]  = "VulkanDesktop/Shader_Generated/Ssao.spv";
constexpr char kHbaoShaderPath[]     = "VulkanDesktop/Shader_Generated/HbaoPlus.spv";
constexpr char kGtaoShaderPath[]     = "VulkanDesktop/Shader_Generated/Gtao.spv";
constexpr char kUpsampleShaderPath[] = "VulkanDesktop/Shader_Generated/AoUpsample.spv";
constexpr char kBlurShaderPath[]     = "VulkanDesktop/Shader_Generated/SsaoBlur.spv";
constexpr char kTemporalShaderPath[] = "VulkanDesktop/Shader_Generated/AoTemporal.spv";

VkExtent2D HalfExtent( uint32_t aWidth, uint32_t aHeight ) {
    return { std::max( 1u, ( aWidth + 1u ) / 2u ), std::max( 1u, ( aHeight + 1u ) / 2u ) };
}

void ClearVkMirrors( Vk_AoState& aState ) {
    aState.myClassicPipeline        = VK_NULL_HANDLE;
    aState.myHbaoPipeline           = VK_NULL_HANDLE;
    aState.myGtaoPipeline           = VK_NULL_HANDLE;
    aState.myUpsamplePipeline       = VK_NULL_HANDLE;
    aState.myBlurPipeline           = VK_NULL_HANDLE;
    aState.myTemporalPipeline       = VK_NULL_HANDLE;
    aState.myClassicPipelineLayout  = VK_NULL_HANDLE;
    aState.myHalfResPipelineLayout  = VK_NULL_HANDLE;
    aState.myUpsamplePipelineLayout = VK_NULL_HANDLE;
    aState.myBlurPipelineLayout     = VK_NULL_HANDLE;
    aState.myTemporalPipelineLayout = VK_NULL_HANDLE;
    aState.myClassicSetLayout       = VK_NULL_HANDLE;
    aState.myHalfResSetLayout       = VK_NULL_HANDLE;
    aState.myUpsampleSetLayout      = VK_NULL_HANDLE;
    aState.myBlurSetLayout          = VK_NULL_HANDLE;
    aState.myTemporalSetLayout      = VK_NULL_HANDLE;
    aState.myDescriptorPool         = VK_NULL_HANDLE;
    aState.myGBufferSampler         = VK_NULL_HANDLE;
    aState.myAoRaw                  = {};
    aState.myAoHalf                 = {};
    aState.myBentNormalHalf         = {};
    aState.myAoBlur                 = {};
    aState.myAoHistory[ 0 ]         = {};
    aState.myAoHistory[ 1 ]         = {};
}

void SyncTextureMirror( Rhi_Device& aRhi, Rhi_Texture aTexture, Vk_TextureResource& aOut ) {
    aOut.Image()     = RhiVulkan::TextureGetVkImage( aRhi, aTexture );
    aOut.ImageView() = RhiVulkan::TextureGetVkView( aRhi, aTexture );
}

void SyncVkMirrors( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    Rhi_Device& rhi   = aCore.myGfxRhiDevice;
    const auto& gfx   = state.myGfx;

    state.myClassicPipeline        = RhiVulkan::PipelineGetVk( rhi, gfx.myClassicPipeline );
    state.myHbaoPipeline           = RhiVulkan::PipelineGetVk( rhi, gfx.myHbaoPipeline );
    state.myGtaoPipeline           = RhiVulkan::PipelineGetVk( rhi, gfx.myGtaoPipeline );
    state.myUpsamplePipeline       = RhiVulkan::PipelineGetVk( rhi, gfx.myUpsamplePipeline );
    state.myBlurPipeline           = RhiVulkan::PipelineGetVk( rhi, gfx.myBlurPipeline );
    state.myTemporalPipeline       = RhiVulkan::PipelineGetVk( rhi, gfx.myTemporalPipeline );
    state.myClassicPipelineLayout  = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myClassicLayout );
    state.myHalfResPipelineLayout  = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myHalfResLayout );
    state.myUpsamplePipelineLayout = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myUpsampleLayout );
    state.myBlurPipelineLayout     = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myBlurLayout );
    state.myTemporalPipelineLayout = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myTemporalLayout );
    state.myClassicSetLayout       = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.myClassicSetLayout );
    state.myHalfResSetLayout       = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.myHalfResSetLayout );
    state.myUpsampleSetLayout      = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.myUpsampleSetLayout );
    state.myBlurSetLayout          = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.myBlurSetLayout );
    state.myTemporalSetLayout      = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.myTemporalSetLayout );
    state.myDescriptorPool         = RhiVulkan::DescriptorPoolGetVk( rhi, gfx.myPool );
    state.myGBufferSampler         = RhiVulkan::SamplerGetVk( rhi, gfx.myGBufferSampler );

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        state.myClassicDescriptorSets[ i ]   = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myClassicSets[ i ] );
        state.myHalfResDescriptorSets[ i ]   = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myHalfResSets[ i ] );
        state.myUpsampleDescriptorSets[ i ]  = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myUpsampleSets[ i ] );
        state.myBlurHorizDescriptorSets[ i ] = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myBlurHorizSets[ i ] );
        state.myBlurVertDescriptorSets[ i ]  = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myBlurVertSets[ i ] );
        state.myTemporalDescriptorSets[ i ]  = RhiVulkan::DescriptorSetGetVk( rhi, gfx.myTemporalSets[ i ] );
    }

    SyncTextureMirror( rhi, gfx.myAoRaw, state.myAoRaw );
    SyncTextureMirror( rhi, gfx.myAoHalf, state.myAoHalf );
    SyncTextureMirror( rhi, gfx.myBentNormalHalf, state.myBentNormalHalf );
    SyncTextureMirror( rhi, gfx.myAoBlur, state.myAoBlur );
    SyncTextureMirror( rhi, gfx.myAoHistory0, state.myAoHistory[ 0 ] );
    SyncTextureMirror( rhi, gfx.myAoHistory1, state.myAoHistory[ 1 ] );
}

bool CreateOrRefreshImages( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    Rhi_Device& rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return false;
    }

    Gfx_AoPass::ImageInitDesc imageDesc{};
    imageDesc.myWidth          = width;
    imageDesc.myHeight         = height;
    imageDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_AoPass::CreateOrRecreateImages( rhi, imageDesc, state.myGfx ) ) {
        return false;
    }

    SyncVkMirrors( aCore );
    Vk_AoPassDetail::gBentHalfLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    UtilLogger::Info( "AO", "targets: full=" + std::to_string( width ) + "x" + std::to_string( height ) + " half=" + std::to_string( HalfExtent( width, height ).width ) + "x"
                                + std::to_string( HalfExtent( width, height ).height ) + " format=R8_UNORM (Gfx)" );
    return true;
}

void CreatePipelines( Vk_Renderer& aCore ) {
    const std::string         classicSpv    = UtilLoader::ResolvePath( aCore.EngineConfig(), kClassicShaderPath );
    const std::string         hbaoSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kHbaoShaderPath );
    const std::string         gtaoSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kGtaoShaderPath );
    const std::string         upsampleSpv   = UtilLoader::ResolvePath( aCore.EngineConfig(), kUpsampleShaderPath );
    const std::string         blurSpv       = UtilLoader::ResolvePath( aCore.EngineConfig(), kBlurShaderPath );
    const std::string         temporalSpv   = UtilLoader::ResolvePath( aCore.EngineConfig(), kTemporalShaderPath );
    const std::vector< char > classicBytes  = LoadSpirvBytes( classicSpv );
    const std::vector< char > hbaoBytes     = LoadSpirvBytes( hbaoSpv );
    const std::vector< char > gtaoBytes     = LoadSpirvBytes( gtaoSpv );
    const std::vector< char > upsampleBytes = LoadSpirvBytes( upsampleSpv );
    const std::vector< char > blurBytes     = LoadSpirvBytes( blurSpv );
    const std::vector< char > temporalBytes = LoadSpirvBytes( temporalSpv );

    Gfx_AoPass::PipelineInitDesc pipeDesc{};
    pipeDesc.myClassicSpirvCode   = classicBytes.data();
    pipeDesc.myClassicSpirvBytes  = classicBytes.size();
    pipeDesc.myHbaoSpirvCode      = hbaoBytes.data();
    pipeDesc.myHbaoSpirvBytes     = hbaoBytes.size();
    pipeDesc.myGtaoSpirvCode      = gtaoBytes.data();
    pipeDesc.myGtaoSpirvBytes     = gtaoBytes.size();
    pipeDesc.myUpsampleSpirvCode  = upsampleBytes.data();
    pipeDesc.myUpsampleSpirvBytes = upsampleBytes.size();
    pipeDesc.myBlurSpirvCode      = blurBytes.data();
    pipeDesc.myBlurSpirvBytes     = blurBytes.size();
    pipeDesc.myTemporalSpirvCode  = temporalBytes.data();
    pipeDesc.myTemporalSpirvBytes = temporalBytes.size();
    pipeDesc.myFramesInFlight     = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_AoPass::CreatePipelines( aCore.myGfxRhiDevice, pipeDesc, aCore.myAoState.myGfx ) ) {
        throw std::runtime_error( "Vk_AoPass: Gfx CreatePipelines failed" );
    }
    SyncVkMirrors( aCore );
    UtilLogger::Info( "PIPELINE", "AO compute pipelines created (Classic SSAO, HBAO+, GTAO, upsample, blur) — Gfx." );
}

struct ClassicAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::mat4 viewProj;
    alignas( 16 ) glm::vec4 params;
    alignas( 8 ) glm::vec2 screenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( ClassicAoPushConstants ) == 224, "ClassicAoPushConstants must match Ssao.comp push block" );

// Shared by HbaoPlus.comp and Gtao.comp (same bindings; params.xy/z/w meaning differs per shader).
struct HalfResAoPushConstants {
    alignas( 16 ) glm::mat4 view;
    alignas( 16 ) glm::mat4 proj;
    alignas( 16 ) glm::vec4 params;  // HBAO: radius,bias,enabled,0 — GTAO: radius,0,enabled,falloff
    alignas( 8 ) glm::uvec2 sliceCount;
    alignas( 8 ) glm::uvec2 stepsPerSlice;
    alignas( 8 ) glm::vec2 halfScreenSize;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( HalfResAoPushConstants ) == 176, "HalfResAoPushConstants must match half-res AO compute shaders" );

struct AoUpsamplePushConstants {
    alignas( 8 ) glm::vec2 fullScreenSize;
    float depthSigma;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoUpsamplePushConstants ) == 24, "AoUpsamplePushConstants must match AoUpsample.comp push block" );

struct AoBlurPushConstants {
    uint32_t axisX;
    uint32_t axisY;
};

static_assert( sizeof( AoBlurPushConstants ) == 8, "AoBlurPushConstants must match SsaoBlur.comp push block" );

struct AoTemporalPushConstants {
    alignas( 4 ) float historyBlend;
    alignas( 4 ) float historyValid;
    alignas( 8 ) glm::vec2 pad;
};

static_assert( sizeof( AoTemporalPushConstants ) == 16, "AoTemporalPushConstants must match AoTemporal.comp push block" );

void UpdateClassicDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler     = state.myGBufferSampler;
    normalInfo.imageView   = aCore.myGBufferState.myNormalRoughness.ImageView();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo worldPosInfo{};
    worldPosInfo.sampler     = state.myGBufferSampler;
    worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
    worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo aoOutInfo{};
    aoOutInfo.imageView   = state.myAoRaw.ImageView();
    aoOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myClassicDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myClassicDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myClassicDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myClassicDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &aoOutInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateHalfResDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler     = state.myGBufferSampler;
    normalInfo.imageView   = aCore.myGBufferState.myNormalRoughness.ImageView();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo worldPosInfo{};
    worldPosInfo.sampler     = state.myGBufferSampler;
    worldPosInfo.imageView   = aCore.myGBufferState.myWorldPosition.ImageView();
    worldPosInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo aoHalfInfo{};
    aoHalfInfo.imageView   = state.myAoHalf.ImageView();
    aoHalfInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo bentHalfInfo{};
    bentHalfInfo.imageView   = state.myBentNormalHalf.ImageView();
    bentHalfInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 5 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myHalfResDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myHalfResDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myHalfResDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &worldPosInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myHalfResDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &aoHalfInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myHalfResDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &bentHalfInfo, 4, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateUpsampleDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;

    VkDescriptorImageInfo halfInfo{};
    halfInfo.sampler     = state.myGBufferSampler;
    halfInfo.imageView   = state.myAoHalf.ImageView();
    halfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo aoOutInfo{};
    aoOutInfo.imageView   = state.myAoRaw.ImageView();
    aoOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 3 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myUpsampleDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &halfInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myUpsampleDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myUpsampleDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &aoOutInfo, 2, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateBlurDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex, VkDescriptorSet aSet, VkImageView aSrcView, VkImageView aDstView ) {
    VkDescriptorImageInfo srcInfo{};
    srcInfo.imageView   = aSrcView;
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = aDstView;
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 2 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( aSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstInfo, 1, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateTemporalDescriptorSet( Vk_Renderer& aCore, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;

    const uint32_t readIndex  = state.myTemporalReadIndex % 2u;
    const uint32_t writeIndex = ( readIndex + 1u ) % 2u;

    VkDescriptorImageInfo mvInfo{};
    mvInfo.sampler     = state.myGBufferSampler;
    mvInfo.imageView   = aCore.myGBufferState.myMotionVector.ImageView();
    mvInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo currentAoInfo{};
    currentAoInfo.sampler     = state.myGBufferSampler;
    currentAoInfo.imageView   = state.myAoRaw.ImageView();
    currentAoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo historyInfo{};
    historyInfo.sampler     = state.myGBufferSampler;
    historyInfo.imageView   = state.myAoHistory[ readIndex ].ImageView();
    historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outInfo{};
    outInfo.imageView   = state.myAoHistory[ writeIndex ].ImageView();
    outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::array< VkWriteDescriptorSet, 4 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &mvInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &currentAoInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &historyInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myTemporalDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outInfo, 3, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void UpdateAllDescriptorSets( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        UpdateClassicDescriptorSet( aCore, i );
        UpdateHalfResDescriptorSet( aCore, i );
        UpdateUpsampleDescriptorSet( aCore, i );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurHorizDescriptorSets[ i ], state.myAoRaw.ImageView(), state.myAoBlur.ImageView() );
        UpdateBlurDescriptorSet( aCore, i, state.myBlurVertDescriptorSets[ i ], state.myAoBlur.ImageView(), state.myAoRaw.ImageView() );
        UpdateTemporalDescriptorSet( aCore, i );
    }
}

}  // namespace

namespace Vk_AoPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myAoState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_AoPass::Destroy( aCore.myGfxRhiDevice, aCore.myAoState.myGfx );
    }
    ClearVkMirrors( aCore.myAoState );
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        aCore.myAoState.myClassicDescriptorSets[ i ]   = VK_NULL_HANDLE;
        aCore.myAoState.myHalfResDescriptorSets[ i ]   = VK_NULL_HANDLE;
        aCore.myAoState.myUpsampleDescriptorSets[ i ]  = VK_NULL_HANDLE;
        aCore.myAoState.myBlurHorizDescriptorSets[ i ] = VK_NULL_HANDLE;
        aCore.myAoState.myBlurVertDescriptorSets[ i ]  = VK_NULL_HANDLE;
        aCore.myAoState.myTemporalDescriptorSets[ i ]  = VK_NULL_HANDLE;
    }
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    aCore.myAoState.myInitialized          = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myAoState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshImages( aCore ) ) {
        throw std::runtime_error( "Vk_AoPass: recreate images failed" );
    }
    Vk_AoPassDetail::ResetImageLayouts();
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myAoState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_AoPass: myGfxRhiDevice required for Gfx Init" );
    }
    UtilLogger::Info( "FG", "Vk_AoPass::Init (Gfx create)." );
    CreatePipelines( aCore );
    if ( !CreateOrRefreshImages( aCore ) ) {
        Gfx_AoPass::Destroy( aCore.myGfxRhiDevice, aCore.myAoState.myGfx );
        ClearVkMirrors( aCore.myAoState );
        throw std::runtime_error( "Vk_AoPass: Gfx CreateOrRecreateImages failed" );
    }
    UpdateAllDescriptorSets( aCore );
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    aCore.myAoState.myInitialized          = true;
}

VkImageView GetRawAoImageView( const Vk_Renderer& aCore ) {
    if ( aCore.myAoState.myInitialized ) {
        return aCore.myAoState.myAoRaw.ImageView();
    }
    return VK_NULL_HANDLE;
}

VkImageView GetBentNormalHalfView( const Vk_Renderer& aCore ) {
    if ( aCore.myAoState.myInitialized && aCore.myAoState.myBentNormalHalf.ImageView() != VK_NULL_HANDLE ) {
        return aCore.myAoState.myBentNormalHalf.ImageView();
    }
    return VK_NULL_HANDLE;
}

void NoteRawAoLayout( VkImageLayout aLayout ) {
    Vk_AoPassDetail::gAoRawLayout = aLayout;
}

VkImageLayout GetRawAoLayout() {
    return Vk_AoPassDetail::gAoRawLayout;
}

namespace {

    Rhi_ImageLayout AoToRhiLayout( VkImageLayout aLayout ) {
        return Vk_FrameCmd::ImageLayoutFromVk( aLayout );
    }

    VkImageLayout AoToVkLayout( Rhi_ImageLayout aLayout ) {
        return Vk_FrameCmd::ImageLayoutToVk( aLayout );
    }

    void PrepareTemporalDescriptorsThunk( void* aUser, uint32_t aFrameIndex ) {
        UpdateTemporalDescriptorSet( *static_cast< Vk_Renderer* >( aUser ), aFrameIndex );
    }

    Gfx_AoPass::GpuResources BuildAoGpuResources( Vk_Renderer& aCore, Rhi_Device& aRhiDevice, uint32_t aFrameIndex ) {
        Vk_AoState&              state = aCore.myAoState;
        Gfx_AoPass::GpuResources gpu{};

        gpu.myClassicPipeline  = RhiVulkan::PipelineAdopt( state.myClassicPipeline );
        gpu.myHbaoPipeline     = RhiVulkan::PipelineAdopt( state.myHbaoPipeline );
        gpu.myGtaoPipeline     = RhiVulkan::PipelineAdopt( state.myGtaoPipeline );
        gpu.myUpsamplePipeline = RhiVulkan::PipelineAdopt( state.myUpsamplePipeline );
        gpu.myBlurPipeline     = RhiVulkan::PipelineAdopt( state.myBlurPipeline );
        gpu.myTemporalPipeline = RhiVulkan::PipelineAdopt( state.myTemporalPipeline );

        gpu.myClassicLayout  = RhiVulkan::PipelineLayoutAdopt( state.myClassicPipelineLayout );
        gpu.myHalfResLayout  = RhiVulkan::PipelineLayoutAdopt( state.myHalfResPipelineLayout );
        gpu.myUpsampleLayout = RhiVulkan::PipelineLayoutAdopt( state.myUpsamplePipelineLayout );
        gpu.myBlurLayout     = RhiVulkan::PipelineLayoutAdopt( state.myBlurPipelineLayout );
        gpu.myTemporalLayout = RhiVulkan::PipelineLayoutAdopt( state.myTemporalPipelineLayout );

        gpu.myClassicSet   = RhiVulkan::DescriptorSetAdopt( state.myClassicDescriptorSets[ aFrameIndex ] );
        gpu.myHalfResSet   = RhiVulkan::DescriptorSetAdopt( state.myHalfResDescriptorSets[ aFrameIndex ] );
        gpu.myUpsampleSet  = RhiVulkan::DescriptorSetAdopt( state.myUpsampleDescriptorSets[ aFrameIndex ] );
        gpu.myBlurHorizSet = RhiVulkan::DescriptorSetAdopt( state.myBlurHorizDescriptorSets[ aFrameIndex ] );
        gpu.myBlurVertSet  = RhiVulkan::DescriptorSetAdopt( state.myBlurVertDescriptorSets[ aFrameIndex ] );
        gpu.myTemporalSet  = RhiVulkan::DescriptorSetAdopt( state.myTemporalDescriptorSets[ aFrameIndex ] );

        gpu.myAoRaw           = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoRaw.Image(), state.myAoRaw.ImageView(), VK_FORMAT_R8_UNORM, 1 );
        gpu.myAoHalf          = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHalf.Image(), state.myAoHalf.ImageView(), VK_FORMAT_R8_UNORM, 1 );
        gpu.myBentNormalHalf  = RhiVulkan::TextureAdopt( aRhiDevice, state.myBentNormalHalf.Image(), state.myBentNormalHalf.ImageView(), VK_FORMAT_R8G8_UNORM, 1 );
        gpu.myAoBlur          = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoBlur.Image(), state.myAoBlur.ImageView(), VK_FORMAT_R8_UNORM, 1 );
        gpu.myAoHistory0      = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHistory[ 0 ].Image(), state.myAoHistory[ 0 ].ImageView(), VK_FORMAT_R8_UNORM, 1 );
        gpu.myAoHistory1      = RhiVulkan::TextureAdopt( aRhiDevice, state.myAoHistory[ 1 ].Image(), state.myAoHistory[ 1 ].ImageView(), VK_FORMAT_R8_UNORM, 1 );
        gpu.myGBufferNormal   = RhiVulkan::TextureAdopt( aRhiDevice, aCore.myGBufferState.myNormalRoughness.Image(), aCore.myGBufferState.myNormalRoughness.ImageView(),
                                                         VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
        gpu.myGBufferWorldPos = RhiVulkan::TextureAdopt( aRhiDevice, aCore.myGBufferState.myWorldPosition.Image(), aCore.myGBufferState.myWorldPosition.ImageView(),
                                                         VK_FORMAT_R16G16B16A16_SFLOAT, 1 );
        return gpu;
    }

    void ReleaseAdoptedAoTextures( Rhi_Device& aRhiDevice, Gfx_AoPass::GpuResources& aGpu ) {
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoRaw );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHalf );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myBentNormalHalf );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoBlur );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHistory0 );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myAoHistory1 );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferNormal );
        Rhi::DeviceDestroyTexture( aRhiDevice, aGpu.myGBufferWorldPos );
    }

}  // namespace

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_AoState& state = aCore.myAoState;
    if ( !state.myInitialized || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return;
    }

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_AoPass::GpuResources gpu = BuildAoGpuResources( aCore, aCore.myGfxRhiDevice, aFrameIndex );

    Gfx_AoPass::LayoutState layouts{};
    layouts.myAoRaw    = AoToRhiLayout( Vk_AoPassDetail::gAoRawLayout );
    layouts.myAoHalf   = AoToRhiLayout( Vk_AoPassDetail::gAoHalfLayout );
    layouts.myBentHalf = AoToRhiLayout( Vk_AoPassDetail::gBentHalfLayout );
    layouts.myAoBlur   = AoToRhiLayout( Vk_AoPassDetail::gAoBlurLayout );

    Gfx_AoPass::RecordInput input{};
    input.mySettings                   = aCore.myAoSettings;
    input.myView                       = aCore.myPrimaryCamera.myView;
    input.myProj                       = aCore.myPrimaryCamera.myProj;
    input.myWidth                      = width;
    input.myHeight                     = height;
    input.myFrameIndex                 = aFrameIndex;
    input.myDebugLabels                = aCore.AreCommandDebugLabelsEnabled();
    input.myContactSoftActive          = aCore.myAoSettings.myContactSoftEnabled && aCore.myShadowAoSoftState.myInitialized;
    input.mySharedTemporalHistoryValid = aCore.myTemporalState.myHistoryValid;
    input.myTemporalReadIndex          = &state.myTemporalReadIndex;
    input.myTemporalHistoryReady       = &state.myTemporalHistoryReady;
    input.myLayouts                    = &layouts;
    input.myPrepareTemporalDescriptors = &PrepareTemporalDescriptorsThunk;
    input.myPrepareTemporalUser        = &aCore;

    Gfx_AoPass::Record( frameCmd.Get(), gpu, input );

    Vk_AoPassDetail::gAoRawLayout    = AoToVkLayout( layouts.myAoRaw );
    Vk_AoPassDetail::gAoHalfLayout   = AoToVkLayout( layouts.myAoHalf );
    Vk_AoPassDetail::gBentHalfLayout = AoToVkLayout( layouts.myBentHalf );
    Vk_AoPassDetail::gAoBlurLayout   = AoToVkLayout( layouts.myAoBlur );

    ReleaseAdoptedAoTextures( aCore.myGfxRhiDevice, gpu );
}

}  // namespace Vk_AoPass

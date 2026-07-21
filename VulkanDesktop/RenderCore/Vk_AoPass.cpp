// Module: Vk_AoPass — pluggable screen-space AO (Classic SSAO, HBAO+, GTAO).
// Outputs linear R8 myAoRaw; ShadowAoSoft and deferred read via GetRawAoImageView().
#include "Vk_AoPass.h"

#include "Vk_AoPassInternal.h"

#include "../Gfx/Gfx_AoMethod.h"
#include "../Gfx/Gfx_AoSettings.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_Initializer.h"
#include "Vk_Renderer.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace {

constexpr char     kClassicShaderPath[]  = "VulkanDesktop/Shader_Generated/Ssao.spv";
constexpr char     kHbaoShaderPath[]     = "VulkanDesktop/Shader_Generated/HbaoPlus.spv";
constexpr char     kGtaoShaderPath[]     = "VulkanDesktop/Shader_Generated/Gtao.spv";
constexpr char     kUpsampleShaderPath[] = "VulkanDesktop/Shader_Generated/AoUpsample.spv";
constexpr char     kBlurShaderPath[]     = "VulkanDesktop/Shader_Generated/SsaoBlur.spv";
constexpr char     kTemporalShaderPath[] = "VulkanDesktop/Shader_Generated/AoTemporal.spv";
constexpr VkFormat kAoFormat             = VK_FORMAT_R8_UNORM;
constexpr VkFormat kBentNormalFormat     = VK_FORMAT_R8G8_UNORM;

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

VkExtent2D HalfExtent( uint32_t aWidth, uint32_t aHeight ) {
    return { std::max( 1u, ( aWidth + 1u ) / 2u ), std::max( 1u, ( aHeight + 1u ) / 2u ) };
}

void DestroyAoTexture( Vk_Renderer& aCore, Vk_TextureResource& aTexture ) {
    const VkDevice     device    = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator allocator = aCore.myRhi.myDeviceCtx.myAllocator;
    if ( aTexture.ImageView() != VK_NULL_HANDLE ) {
        vkDestroyImageView( device, aTexture.ImageView(), nullptr );
        aTexture.ImageView() = VK_NULL_HANDLE;
    }
    if ( aTexture.Image() != VK_NULL_HANDLE ) {
        vmaDestroyImage( allocator, aTexture.Image(), aTexture.Allocation() );
        aTexture.AllocImage() = {};
    }
}

void DestroyAoImages( Vk_Renderer& aCore ) {
    DestroyAoTexture( aCore, aCore.myAoState.myAoRaw );
    DestroyAoTexture( aCore, aCore.myAoState.myAoHalf );
    DestroyAoTexture( aCore, aCore.myAoState.myBentNormalHalf );
    DestroyAoTexture( aCore, aCore.myAoState.myAoBlur );
    DestroyAoTexture( aCore, aCore.myAoState.myAoHistory[ 0 ] );
    DestroyAoTexture( aCore, aCore.myAoState.myAoHistory[ 1 ] );
    Vk_AoPassDetail::ResetImageLayouts();
}

void CreateAoImage( Vk_Renderer& aCore, VkExtent2D aExtent, Vk_TextureResource& aTexture ) {
    if ( aExtent.width == 0 || aExtent.height == 0 ) {
        return;
    }

    aCore.CreateImage( aExtent, kAoFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, aTexture.AllocImage() );
    aTexture.ImageView() = aCore.CreateImageView( aTexture.Image(), kAoFormat, VK_IMAGE_ASPECT_COLOR_BIT );
}

void CreateAoImages( Vk_Renderer& aCore ) {
    const VkExtent2D full = aCore.mySwapchainCtx.mySwapChainExtent;
    CreateAoImage( aCore, full, aCore.myAoState.myAoRaw );
    CreateAoImage( aCore, full, aCore.myAoState.myAoBlur );
    CreateAoImage( aCore, full, aCore.myAoState.myAoHistory[ 0 ] );
    CreateAoImage( aCore, full, aCore.myAoState.myAoHistory[ 1 ] );
    CreateAoImage( aCore, HalfExtent( full.width, full.height ), aCore.myAoState.myAoHalf );
    aCore.CreateImage( HalfExtent( full.width, full.height ), kBentNormalFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VMA_MEMORY_USAGE_GPU_ONLY, 1, VK_SAMPLE_COUNT_1_BIT, aCore.myAoState.myBentNormalHalf.AllocImage() );
    aCore.myAoState.myBentNormalHalf.ImageView() = aCore.CreateImageView( aCore.myAoState.myBentNormalHalf.Image(), kBentNormalFormat, VK_IMAGE_ASPECT_COLOR_BIT );
    // Classic SSAO may never write bent normals; deferred still samples binding 18 as SHADER_READ_ONLY.
    aCore.TransitionImageLayout( aCore.myAoState.myBentNormalHalf.Image(), kBentNormalFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1 );
    Vk_AoPassDetail::gBentHalfLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    UtilLogger::Info( "AO", "targets: full=" + std::to_string( full.width ) + "x" + std::to_string( full.height )
                                + " half=" + std::to_string( HalfExtent( full.width, full.height ).width ) + "x"
                                + std::to_string( HalfExtent( full.width, full.height ).height ) + " format=R8_UNORM"
                                + " bent=" + std::to_string( reinterpret_cast< uintptr_t >( aCore.myAoState.myBentNormalHalf.Image() ) )
                                + " raw=" + std::to_string( reinterpret_cast< uintptr_t >( aCore.myAoState.myAoRaw.Image() ) ) );
}

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

VkPipelineLayout CreateComputePipelineLayout( Vk_Renderer& aCore, VkDescriptorSetLayout aSetLayout, VkPushConstantRange aPushRange ) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &aSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &aPushRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &layout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create pipeline layout" );
    }
    return layout;
}

VkPipeline CreateComputePipelineWithLayout( Vk_Renderer& aCore, const std::string& aSpvPath, VkPipelineLayout aLayout ) {
    VkShaderModule computeModule = aCore.CreateShaderModule( aSpvPath );

    const VkPipelineShaderStageCreateInfo stageInfo = VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_COMPUTE_BIT, computeModule, "main" );

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = aLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if ( vkCreateComputePipelines( aCore.myRhi.myDeviceCtx.myDevice, aCore.myRhi.myDeviceCtx.myPipelineCache, 1, &pipelineInfo, nullptr, &pipeline ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create compute pipeline" );
    }

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, computeModule, nullptr );
    return pipeline;
}

VkPipeline CreateComputePipeline( Vk_Renderer& aCore, const std::string& aSpvPath, VkDescriptorSetLayout aSetLayout, VkPipelineLayout& aOutLayout,
                                  VkPushConstantRange aPushRange ) {
    aOutLayout = CreateComputePipelineLayout( aCore, aSetLayout, aPushRange );
    return CreateComputePipelineWithLayout( aCore, aSpvPath, aOutLayout );
}

void CreatePipelines( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;

    const std::array< VkDescriptorSetLayoutBinding, 4 > classicBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
    };
    VkDescriptorSetLayoutCreateInfo classicLayoutInfo{};
    classicLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    classicLayoutInfo.bindingCount = static_cast< uint32_t >( classicBindings.size() );
    classicLayoutInfo.pBindings    = classicBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &classicLayoutInfo, nullptr, &state.myClassicSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create classic AO descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 5 > halfResBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 4 ),
    };
    VkDescriptorSetLayoutCreateInfo halfResLayoutInfo{};
    halfResLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    halfResLayoutInfo.bindingCount = static_cast< uint32_t >( halfResBindings.size() );
    halfResLayoutInfo.pBindings    = halfResBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &halfResLayoutInfo, nullptr, &state.myHalfResSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create half-res AO descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 3 > upsampleBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
    };
    VkDescriptorSetLayoutCreateInfo upsampleLayoutInfo{};
    upsampleLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    upsampleLayoutInfo.bindingCount = static_cast< uint32_t >( upsampleBindings.size() );
    upsampleLayoutInfo.pBindings    = upsampleBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &upsampleLayoutInfo, nullptr, &state.myUpsampleSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create AO upsample descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 2 > blurBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
    };
    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast< uint32_t >( blurBindings.size() );
    blurLayoutInfo.pBindings    = blurBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &blurLayoutInfo, nullptr, &state.myBlurSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create blur descriptor set layout" );
    }

    const std::array< VkDescriptorSetLayoutBinding, 4 > temporalBindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3 ),
    };
    VkDescriptorSetLayoutCreateInfo temporalLayoutInfo{};
    temporalLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    temporalLayoutInfo.bindingCount = static_cast< uint32_t >( temporalBindings.size() );
    temporalLayoutInfo.pBindings    = temporalBindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myRhi.myDeviceCtx.myDevice, &temporalLayoutInfo, nullptr, &state.myTemporalSetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create temporal AO descriptor set layout" );
    }

    VkPushConstantRange classicPushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( ClassicAoPushConstants ) };
    state.myClassicPipeline = CreateComputePipeline( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kClassicShaderPath ), state.myClassicSetLayout,
                                                     state.myClassicPipelineLayout, classicPushRange );

    VkPushConstantRange halfResPushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( HalfResAoPushConstants ) };
    state.myHalfResPipelineLayout = CreateComputePipelineLayout( aCore, state.myHalfResSetLayout, halfResPushRange );
    state.myHbaoPipeline          = CreateComputePipelineWithLayout( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kHbaoShaderPath ), state.myHalfResPipelineLayout );
    state.myGtaoPipeline          = CreateComputePipelineWithLayout( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kGtaoShaderPath ), state.myHalfResPipelineLayout );

    VkPushConstantRange upsamplePushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( AoUpsamplePushConstants ) };
    state.myUpsamplePipeline = CreateComputePipeline( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kUpsampleShaderPath ), state.myUpsampleSetLayout,
                                                      state.myUpsamplePipelineLayout, upsamplePushRange );

    VkPushConstantRange blurPushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( AoBlurPushConstants ) };
    state.myBlurPipeline =
        CreateComputePipeline( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kBlurShaderPath ), state.myBlurSetLayout, state.myBlurPipelineLayout, blurPushRange );

    VkPushConstantRange temporalPushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( AoTemporalPushConstants ) };
    state.myTemporalPipeline = CreateComputePipeline( aCore, UtilLoader::ResolvePath( aCore.EngineConfig(), kTemporalShaderPath ), state.myTemporalSetLayout,
                                                      state.myTemporalPipelineLayout, temporalPushRange );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create G-buffer sampler" );
    }

    std::array< VkDescriptorPoolSize, 2 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 11 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 11 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = MAX_FRAMES_IN_FLIGHT * 6;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    if ( vkCreateDescriptorPool( aCore.myRhi.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_AoPass: failed to create descriptor pool" );
    }

    const VkDevice              device                 = aCore.myRhi.myDeviceCtx.myDevice;
    const VkPipeline            classicPipeline        = state.myClassicPipeline;
    const VkPipeline            hbaoPipeline           = state.myHbaoPipeline;
    const VkPipeline            gtaoPipeline           = state.myGtaoPipeline;
    const VkPipeline            upsamplePipeline       = state.myUpsamplePipeline;
    const VkPipeline            blurPipeline           = state.myBlurPipeline;
    const VkPipeline            temporalPipeline       = state.myTemporalPipeline;
    const VkPipelineLayout      classicPipelineLayout  = state.myClassicPipelineLayout;
    const VkPipelineLayout      halfResPipelineLayout  = state.myHalfResPipelineLayout;
    const VkPipelineLayout      upsamplePipelineLayout = state.myUpsamplePipelineLayout;
    const VkPipelineLayout      blurPipelineLayout     = state.myBlurPipelineLayout;
    const VkPipelineLayout      temporalPipelineLayout = state.myTemporalPipelineLayout;
    const VkDescriptorSetLayout classicSetLayout       = state.myClassicSetLayout;
    const VkDescriptorSetLayout halfResSetLayout       = state.myHalfResSetLayout;
    const VkDescriptorSetLayout upsampleSetLayout      = state.myUpsampleSetLayout;
    const VkDescriptorSetLayout blurSetLayout          = state.myBlurSetLayout;
    const VkDescriptorSetLayout temporalSetLayout      = state.myTemporalSetLayout;
    const VkDescriptorPool      descriptorPool         = state.myDescriptorPool;
    const VkSampler             gbufferSampler         = state.myGBufferSampler;
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction( [ device, classicPipeline, hbaoPipeline, gtaoPipeline, upsamplePipeline, blurPipeline, temporalPipeline,
                                                            classicPipelineLayout, halfResPipelineLayout, upsamplePipelineLayout, blurPipelineLayout, temporalPipelineLayout,
                                                            classicSetLayout, halfResSetLayout, upsampleSetLayout, blurSetLayout, temporalSetLayout, descriptorPool,
                                                            gbufferSampler ]() {
        auto destroyPipe = [ device ]( VkPipeline p ) {
            if ( p != VK_NULL_HANDLE ) {
                vkDestroyPipeline( device, p, nullptr );
            }
        };
        destroyPipe( classicPipeline );
        destroyPipe( hbaoPipeline );
        destroyPipe( gtaoPipeline );
        destroyPipe( upsamplePipeline );
        destroyPipe( blurPipeline );
        destroyPipe( temporalPipeline );
        if ( classicPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, classicPipelineLayout, nullptr );
        }
        if ( halfResPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, halfResPipelineLayout, nullptr );
        }
        if ( upsamplePipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, upsamplePipelineLayout, nullptr );
        }
        if ( blurPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, blurPipelineLayout, nullptr );
        }
        if ( temporalPipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, temporalPipelineLayout, nullptr );
        }
        if ( classicSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, classicSetLayout, nullptr );
        }
        if ( halfResSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, halfResSetLayout, nullptr );
        }
        if ( upsampleSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, upsampleSetLayout, nullptr );
        }
        if ( blurSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, blurSetLayout, nullptr );
        }
        if ( temporalSetLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, temporalSetLayout, nullptr );
        }
        if ( descriptorPool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        }
        if ( gbufferSampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, gbufferSampler, nullptr );
        }
    } );

    UtilLogger::Info( "PIPELINE", "AO compute pipelines created (Classic SSAO, HBAO+, GTAO, upsample, blur)." );
}

void AllocateDescriptorSets( Vk_Renderer& aCore ) {
    Vk_AoState& state = aCore.myAoState;
    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;

        allocInfo.pSetLayouts = &state.myClassicSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myClassicDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate classic AO descriptor set" );
        }
        allocInfo.pSetLayouts = &state.myHalfResSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myHalfResDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate half-res AO descriptor set" );
        }
        allocInfo.pSetLayouts = &state.myUpsampleSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myUpsampleDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate AO upsample descriptor set" );
        }
        allocInfo.pSetLayouts = &state.myBlurSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBlurHorizDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate horizontal blur descriptor set" );
        }
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myBlurVertDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate vertical blur descriptor set" );
        }
        allocInfo.pSetLayouts = &state.myTemporalSetLayout;
        if ( vkAllocateDescriptorSets( aCore.myRhi.myDeviceCtx.myDevice, &allocInfo, &state.myTemporalDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_AoPass: failed to allocate temporal AO descriptor set" );
        }
    }
    UpdateAllDescriptorSets( aCore );
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
    DestroyAoImages( aCore );
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
    if ( !aCore.myAoState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    DestroyAoImages( aCore );
    CreateAoImages( aCore );
    aCore.myAoState.myTemporalReadIndex    = 0u;
    aCore.myAoState.myTemporalHistoryReady = false;
    UpdateAllDescriptorSets( aCore );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myAoState.myInitialized ) {
        return;
    }
    UtilLogger::Info( "FG", "Vk_AoPass::Init." );
    CreatePipelines( aCore );
    CreateAoImages( aCore );
    AllocateDescriptorSets( aCore );
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

}  // namespace Vk_AoPass

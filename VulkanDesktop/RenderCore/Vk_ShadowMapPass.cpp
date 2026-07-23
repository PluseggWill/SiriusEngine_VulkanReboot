// Module: Vk_ShadowMapPass — directional shadow depth map (unculled caster list, Khronos compare).
#include "Vk_ShadowMapPass.h"

#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Gfx/Gfx_ShadowMapPass.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_FrameCmd.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include "../Rhi/Rhi_Enums.h"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

constexpr char kShadowVertSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapVert.spv";
constexpr char kShadowFragSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapFrag.spv";

Rhi_Format MapDepthFormat( VkFormat aFormat ) {
    switch ( aFormat ) {
    case VK_FORMAT_D32_SFLOAT:
        return Rhi_Format::D32_Sfloat;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return Rhi_Format::D32_Sfloat_S8_Uint;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return Rhi_Format::D24_Unorm_S8_Uint;
    default:
        return Rhi_Format::Undefined;
    }
}

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

void SyncVkMirrors( Vk_Renderer& aCore ) {
    Vk_ShadowMapState& state = aCore.myShadowMapState;
    Rhi_Device&        rhi   = aCore.myGfxRhiDevice;
    const auto&        gfx   = state.myGfx;

    state.myDepth             = {};
    state.myDepth.Image()     = RhiVulkan::TextureGetVkImage( rhi, gfx.myDepth );
    state.myDepth.ImageView() = RhiVulkan::TextureGetVkView( rhi, gfx.myDepth );
    state.myRenderPass        = RhiVulkan::RenderPassGetVk( rhi, gfx.myRenderPass );
    state.myFramebuffer       = RhiVulkan::FramebufferGetVk( rhi, gfx.myFramebuffer );
    state.myPipeline          = RhiVulkan::PipelineGetVk( rhi, gfx.myPipeline );
    state.myPipelineLayout    = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myLayout );
    state.myCompareSampler    = RhiVulkan::SamplerGetVk( rhi, gfx.myCompareSampler );
    state.myDepthReadSampler  = RhiVulkan::SamplerGetVk( rhi, gfx.myDepthReadSampler );
}

void CreateShadowResources( Vk_Renderer& aCore ) {
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_ShadowMapPass: myGfxRhiDevice required for Gfx Init" );
    }

    const Rhi_Format depthFormat = MapDepthFormat( aCore.FindDepthFormat() );
    if ( depthFormat == Rhi_Format::Undefined ) {
        throw std::runtime_error( "Vk_ShadowMapPass: unsupported depth format" );
    }

    const std::string         vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowVertSpv );
    const std::string         fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowFragSpv );
    const std::vector< char > vertSpv  = LoadSpirvBytes( vertPath );
    const std::vector< char > fragSpv  = LoadSpirvBytes( fragPath );

    Gfx_ShadowMapPass::ResourcesInitDesc desc{};
    desc.myDepthFormat     = depthFormat;
    desc.myObjectSetLayout = RhiVulkan::DescriptorSetLayoutAdopt( aCore.mySceneGpuCtx.myObjectSetLayout );
    desc.myVertSpirv       = vertSpv.data();
    desc.myVertSpirvBytes  = vertSpv.size();
    desc.myFragSpirv       = fragSpv.data();
    desc.myFragSpirvBytes  = fragSpv.size();

    if ( !Gfx_ShadowMapPass::CreateResources( aCore.myGfxRhiDevice, desc, aCore.myShadowMapState.myGfx ) ) {
        throw std::runtime_error( "Vk_ShadowMapPass: Gfx CreateResources failed" );
    }

    SyncVkMirrors( aCore );
    UtilLogger::Info( "PIPELINE",
                      "ShadowMap directional pass created (" + std::to_string( Vk_ShadowMapState::kMapSize ) + "x" + std::to_string( Vk_ShadowMapState::kMapSize ) + ")." );
}

VkImageMemoryBarrier MakeShadowDepthBarrier( VkImage aImage, VkImageLayout aOldLayout, VkImageLayout aNewLayout, VkAccessFlags aSrcAccess, VkAccessFlags aDstAccess ) {
    VkImageMemoryBarrier barrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout        = aOldLayout;
    barrier.newLayout        = aNewLayout;
    barrier.srcAccessMask    = aSrcAccess;
    barrier.dstAccessMask    = aDstAccess;
    barrier.image            = aImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    return barrier;
}

void CmdTransitionShadowDepth( Vk_ShadowMapState& aState, VkCommandBuffer aCommandBuffer, VkImageLayout aNewLayout, VkPipelineStageFlags aDstStageMask,
                               VkAccessFlags aDstAccess ) {
    if ( aState.myDepthLayout == aNewLayout && aDstAccess == 0 ) {
        return;
    }

    VkPipelineStageFlags srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags        srcAccess = 0;
    if ( aState.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
        srcStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        srcAccess = VK_ACCESS_SHADER_READ_BIT;
    }
    else if ( aState.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) {
        srcStage  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    const VkImageMemoryBarrier barrier = MakeShadowDepthBarrier( aState.myDepth.Image(), aState.myDepthLayout, aNewLayout, srcAccess, aDstAccess );
    vkCmdPipelineBarrier( aCommandBuffer, srcStage, aDstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier );
    aState.myDepthLayout = aNewLayout;
}

}  // namespace

namespace Vk_ShadowMapPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myShadowMapState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_ShadowMapPass::DestroyResources( aCore.myGfxRhiDevice, aCore.myShadowMapState.myGfx );
    }
    aCore.myShadowMapState.myDepth            = {};
    aCore.myShadowMapState.myRenderPass       = VK_NULL_HANDLE;
    aCore.myShadowMapState.myFramebuffer      = VK_NULL_HANDLE;
    aCore.myShadowMapState.myPipeline         = VK_NULL_HANDLE;
    aCore.myShadowMapState.myPipelineLayout   = VK_NULL_HANDLE;
    aCore.myShadowMapState.myCompareSampler   = VK_NULL_HANDLE;
    aCore.myShadowMapState.myDepthReadSampler = VK_NULL_HANDLE;
    aCore.myShadowMapState.myInitialized      = false;
    aCore.myShadowMapState.myDepthLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
}

void CmdBarrierForDeferredRead( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer ) {
    if ( !aCore.myShadowMapState.myInitialized ) {
        return;
    }

    Vk_ShadowMapState&         state        = aCore.myShadowMapState;
    const VkPipelineStageFlags shaderStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    if ( state.myDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED ) {
        // Shadow pass skipped (disabled / below horizon) — layout must still be valid for bound samplers.
        CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, shaderStages, VK_ACCESS_SHADER_READ_BIT );
        return;
    }

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    if ( !Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aCore.myLightingSettings.myShadowsEnabled, sunDir ) ) {
        return;
    }

    if ( state.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
        const VkImageMemoryBarrier barrier =
            MakeShadowDepthBarrier( state.myDepth.Image(), state.myDepthLayout, state.myDepthLayout, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, shaderStages, 0, 0, nullptr, 0, nullptr,
                              1, &barrier );
        return;
    }

    CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, shaderStages, VK_ACCESS_SHADER_READ_BIT );
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myShadowMapState.myInitialized ) {
        return;
    }
    if ( aCore.mySceneGpuCtx.myObjectSetLayout == VK_NULL_HANDLE ) {
        throw std::runtime_error( "Vk_ShadowMapPass::Init requires object descriptor layout" );
    }
    CreateShadowResources( aCore );
    aCore.myShadowMapState.myInitialized = true;
    aCore.myShadowMapState.myDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void RecordDraw( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aCasterPass, bool aEmitDebugLabels ) {
    if ( !aCore.myShadowMapState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }

    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    if ( !Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aCore.myLightingSettings.myShadowsEnabled, sunDir ) ) {
        Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( aCore, aCore.myFrameCtx.myCurrentFrame );
        return;
    }

    const Gfx_Bounds                                   sceneBounds = aCore.GetShadowCasterBounds();
    const Gfx_LightingMath::Gfx_DirectionalShadowSetup shadowSetup =
        Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowSetup( sunDir, sceneBounds, Vk_ShadowMapState::kMapSize );

    Vk_ShadowMapState& state  = aCore.myShadowMapState;
    state.myLightViewProj     = shadowSetup.myLightViewProj;
    state.myDepthBiasConstant = shadowSetup.myDepthBiasConstant;
    state.myDepthBiasSlope    = shadowSetup.myDepthBiasSlope;

    CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );

    std::vector< Gfx_ShadowMapPass::DrawItem > draws;
    draws.reserve( aCasterPass.myDraws.size() );
    for ( const Gfx_BatchRun& batch : aCasterPass.myBatchRuns ) {
        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {
            const Gfx_DrawInstance&     draw = aCasterPass.myDraws[ batch.myFirstDrawIndex + drawIndex ];
            const Vk_MeshResource&      mesh = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );
            Gfx_ShadowMapPass::DrawItem item{};
            item.myVertexBuffer  = RhiVulkan::BufferAdopt( mesh.myVertexBuffer.myBuffer );
            item.myIndexBuffer   = RhiVulkan::BufferAdopt( mesh.myIndexBuffer.myBuffer );
            item.myIndexCount    = mesh.myIndexCount;
            item.myDynamicOffset = draw.myInstanceDataOffset;
            draws.push_back( item );
        }
    }

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_ShadowMapPass::GpuResources gpu{};
    gpu.myPipeline    = RhiVulkan::PipelineAdopt( state.myPipeline );
    gpu.myLayout      = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.myObjectSet   = RhiVulkan::DescriptorSetAdopt( aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor );
    gpu.myRenderPass  = RhiVulkan::RenderPassAdopt( state.myRenderPass );
    gpu.myFramebuffer = RhiVulkan::FramebufferAdopt( state.myFramebuffer );

    Gfx_ShadowMapPass::RecordInput input{};
    input.myLightViewProj     = state.myLightViewProj;
    input.myDepthBiasConstant = state.myDepthBiasConstant;
    input.myDepthBiasSlope    = state.myDepthBiasSlope;
    input.myDebugLabels       = aEmitDebugLabels;
    input.myDraws             = draws.data();
    input.myDrawCount         = static_cast< uint32_t >( draws.size() );
    input.myDrawCallsOut      = &aCore.myFrameStats.myDrawCalls;

    Gfx_ShadowMapPass::Record( frameCmd.Get(), gpu, input );

    state.myDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    Vk_FrameUniformUploader::UpdateLightingGlobals( aCore, aCore.myFrameCtx.myCurrentFrame, state.myLightViewProj );
}

}  // namespace Vk_ShadowMapPass

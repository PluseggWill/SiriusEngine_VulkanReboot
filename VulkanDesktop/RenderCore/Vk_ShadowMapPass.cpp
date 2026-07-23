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
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"
#include "Vk_VertexLayout.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <stdexcept>
#include <vector>

namespace {

constexpr char kShadowVertSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapVert.spv";
constexpr char kShadowFragSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapFrag.spv";

// Shadow push-constant block — must match ShadowMap.vert layout.
struct ShadowPushConstants {
    alignas( 16 ) glm::mat4 myLightViewProj;
};

static_assert( sizeof( ShadowPushConstants ) == 64, "ShadowPushConstants must match ShadowMap.vert push block" );

void CreateShadowResources( Vk_Renderer& aCore ) {
    Vk_ShadowMapState& state       = aCore.myShadowMapState;
    const VkFormat     depthFormat = aCore.FindDepthFormat();

    const VkExtent2D extent{ Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    aCore.CreateImage( extent, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 1,
                       VK_SAMPLE_COUNT_1_BIT, state.myDepth.AllocImage() );
    state.myDepth.ImageView() = aCore.CreateImageView( state.myDepth.Image(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT );

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = depthFormat;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &depthAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;
    if ( vkCreateRenderPass( aCore.myRhi.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &state.myRenderPass ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create render pass" );
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = state.myRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments    = &state.myDepth.ImageView();
    framebufferInfo.width           = Vk_ShadowMapState::kMapSize;
    framebufferInfo.height          = Vk_ShadowMapState::kMapSize;
    framebufferInfo.layers          = 1;
    if ( vkCreateFramebuffer( aCore.myRhi.myDeviceCtx.myDevice, &framebufferInfo, nullptr, &state.myFramebuffer ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create framebuffer" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( ShadowPushConstants );

    VkPipelineLayoutCreateInfo layoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    layoutInfo.setLayoutCount             = 1;
    layoutInfo.pSetLayouts                = &aCore.mySceneGpuCtx.myObjectSetLayout;
    layoutInfo.pushConstantRangeCount     = 1;
    layoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myRhi.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create pipeline layout" );
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.compareEnable    = VK_TRUE;
    samplerInfo.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
    samplerInfo.anisotropyEnable = VK_FALSE;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myCompareSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create compare sampler" );
    }

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.magFilter     = VK_FILTER_NEAREST;
    samplerInfo.minFilter     = VK_FILTER_NEAREST;
    if ( vkCreateSampler( aCore.myRhi.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myDepthReadSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create depth read sampler" );
    }

    const std::string vertPath   = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowVertSpv );
    const std::string fragPath   = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowFragSpv );
    VkShaderModule    vertModule = aCore.CreateShaderModule( vertPath );
    VkShaderModule    fragModule = aCore.CreateShaderModule( fragPath );

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo      = VkInit::Pipeline_VertexInputStateCreateInfo();
    auto                                 bindingDescription   = Vk_GetGfxVertexBindingDescription();
    auto                                 attributeDescription = Vk_GetGfxVertexAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();
    pipelineBuilder.myVertexInputInfo                         = vertexInputInfo;
    pipelineBuilder.myInputAssembly                           = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                                = VkInit::ViewportCreateInfo( { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize } );
    pipelineBuilder.myScissor.offset                          = { 0, 0 };
    pipelineBuilder.myScissor.extent                          = { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    // BACK cull: front-face depth (closest to light). FRONT cull stores back faces and causes light leak.
    pipelineBuilder.myRasterizer                    = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT );
    pipelineBuilder.myRasterizer.depthBiasEnable    = VK_TRUE;
    pipelineBuilder.myMultisampling                 = VkInit::Pipeline_MultisampleCreateInfo( VK_SAMPLE_COUNT_1_BIT );
    pipelineBuilder.myDepthStencil                  = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthTestEnable  = VK_TRUE;
    pipelineBuilder.myDepthStencil.depthWriteEnable = VK_TRUE;
    pipelineBuilder.myDepthStencil.depthCompareOp   = VK_COMPARE_OP_GREATER;
    pipelineBuilder.myColorBlendAttachment          = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                = state.myPipelineLayout;
    pipelineBuilder.SetDynamicStates( { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, VK_DYNAMIC_STATE_DEPTH_BIAS } );
    state.myPipeline = pipelineBuilder.BuildPipeline( aCore.myRhi.myDeviceCtx.myDevice, state.myRenderPass, aCore.myRhi.myDeviceCtx.myPipelineCache, nullptr );
    UtilLogger::Info( "PIPELINE", "ShadowMap directional pass created." );

    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myRhi.myDeviceCtx.myDevice, fragModule, nullptr );

    const VkDevice         device           = aCore.myRhi.myDeviceCtx.myDevice;
    const VmaAllocator     allocator        = aCore.myRhi.myDeviceCtx.myAllocator;
    const VkRenderPass     renderPass       = state.myRenderPass;
    const VkFramebuffer    framebuffer      = state.myFramebuffer;
    const VkPipeline       pipeline         = state.myPipeline;
    const VkPipelineLayout layout           = state.myPipelineLayout;
    const VkSampler        compareSampler   = state.myCompareSampler;
    const VkSampler        depthReadSampler = state.myDepthReadSampler;
    const VkImageView      depthView        = state.myDepth.ImageView();
    const VkImage          depthImage       = state.myDepth.Image();
    const VmaAllocation    depthAlloc       = state.myDepth.Allocation();
    aCore.myRhi.myDeviceCtx.myDeletionQueue.pushFunction(
        [ device, allocator, renderPass, framebuffer, pipeline, layout, compareSampler, depthReadSampler, depthView, depthImage, depthAlloc ]() {
            vkDestroySampler( device, depthReadSampler, nullptr );
            vkDestroySampler( device, compareSampler, nullptr );
            vkDestroyPipeline( device, pipeline, nullptr );
            vkDestroyPipelineLayout( device, layout, nullptr );
            vkDestroyFramebuffer( device, framebuffer, nullptr );
            vkDestroyRenderPass( device, renderPass, nullptr );
            vkDestroyImageView( device, depthView, nullptr );
            vmaDestroyImage( allocator, depthImage, depthAlloc );
        } );

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
    aCore.myShadowMapState.myInitialized = false;
    aCore.myShadowMapState.myDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

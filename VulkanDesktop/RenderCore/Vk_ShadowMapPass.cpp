// Module: Vk_ShadowMapPass — directional shadow depth map (unculled caster list, Khronos compare).
#include "Vk_ShadowMapPass.h"

#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"
#include "Vk_Types.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <stdexcept>

namespace {

constexpr char kShadowVertSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapVert.spv";
constexpr char kShadowFragSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapFrag.spv";

// Khronos Vulkan-Samples multithreading_render_passes shadow subpass defaults.
constexpr float kKhronosDepthBiasConstant = -1.4f;
constexpr float kKhronosDepthBiasSlope    = -1.7f;

struct ShadowPushConstants {
    alignas( 16 ) glm::mat4 lightViewProj;
};

static_assert( sizeof( ShadowPushConstants ) == 64, "ShadowPushConstants must match ShadowMap.vert push block" );

void CreateShadowResources( Vk_Core& aCore ) {
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
    if ( vkCreateRenderPass( aCore.myDeviceCtx.myDevice, &renderPassInfo, nullptr, &state.myRenderPass ) != VK_SUCCESS ) {
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
    if ( vkCreateFramebuffer( aCore.myDeviceCtx.myDevice, &framebufferInfo, nullptr, &state.myFramebuffer ) != VK_SUCCESS ) {
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
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create pipeline layout" );
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable    = VK_TRUE;
    samplerInfo.compareOp        = VK_COMPARE_OP_GREATER_OR_EQUAL;
    samplerInfo.anisotropyEnable = VK_FALSE;
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myCompareSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create compare sampler" );
    }

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.magFilter     = VK_FILTER_NEAREST;
    samplerInfo.minFilter     = VK_FILTER_NEAREST;
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myDepthReadSampler ) != VK_SUCCESS ) {
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
    auto                                 bindingDescription   = Gfx_Vertex::getBindingDescription();
    auto                                 attributeDescription = Gfx_Vertex::getAttributeDescriptions();
    vertexInputInfo.vertexBindingDescriptionCount             = 1;
    vertexInputInfo.pVertexBindingDescriptions                = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount           = static_cast< uint32_t >( attributeDescription.size() );
    vertexInputInfo.pVertexAttributeDescriptions              = attributeDescription.data();
    pipelineBuilder.myVertexInputInfo                         = vertexInputInfo;
    pipelineBuilder.myInputAssembly                           = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                                = VkInit::ViewportCreateInfo( { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize } );
    pipelineBuilder.myScissor.offset                          = { 0, 0 };
    pipelineBuilder.myScissor.extent                          = { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    pipelineBuilder.myRasterizer                              = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT );
    pipelineBuilder.myRasterizer.depthBiasEnable              = VK_TRUE;
    pipelineBuilder.myMultisampling                           = VkInit::Pipeline_MultisampleCreateInfo( VK_SAMPLE_COUNT_1_BIT );
    pipelineBuilder.myDepthStencil                            = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthTestEnable            = VK_TRUE;
    pipelineBuilder.myDepthStencil.depthWriteEnable           = VK_TRUE;
    pipelineBuilder.myDepthStencil.depthCompareOp             = VK_COMPARE_OP_GREATER;
    pipelineBuilder.myColorBlendAttachment                    = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                          = state.myPipelineLayout;
    pipelineBuilder.SetDynamicStates( { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, VK_DYNAMIC_STATE_DEPTH_BIAS } );
    state.myPipeline = pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, state.myRenderPass, aCore.myDeviceCtx.myPipelineCache, nullptr );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );

    const VkDevice         device           = aCore.myDeviceCtx.myDevice;
    const VmaAllocator     allocator        = aCore.myDeviceCtx.myAllocator;
    const VkRenderPass     renderPass       = state.myRenderPass;
    const VkFramebuffer    framebuffer      = state.myFramebuffer;
    const VkPipeline       pipeline         = state.myPipeline;
    const VkPipelineLayout layout           = state.myPipelineLayout;
    const VkSampler        compareSampler   = state.myCompareSampler;
    const VkSampler        depthReadSampler = state.myDepthReadSampler;
    const VkImageView      depthView        = state.myDepth.ImageView();
    const VkImage          depthImage       = state.myDepth.Image();
    const VmaAllocation    depthAlloc       = state.myDepth.Allocation();
    aCore.myDeviceCtx.myDeletionQueue.pushFunction(
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

void RecordShadowDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, bool aEmitDebugLabels ) {
    const VkDescriptorSet  objectDescriptor = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ].myObjectDescriptor;
    const VkPipelineLayout layout           = aCore.myShadowMapState.myPipelineLayout;

    ShadowPushConstants push{};
    push.lightViewProj = aCore.myShadowMapState.myLightViewProj;
    vkCmdPushConstants( aCommandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof( push ), &push );

    for ( const Gfx_BatchRun& batch : aPass.myBatchRuns ) {
        for ( uint32_t drawIndex = 0; drawIndex < batch.myDrawCount; ++drawIndex ) {
            const uint32_t          absoluteDrawIndex = batch.myFirstDrawIndex + drawIndex;
            const Gfx_DrawInstance& draw              = aPass.myDraws[ absoluteDrawIndex ];
            const Gfx_Mesh&         mesh              = aCore.mySceneGpuCtx.myResourceTables.GetMesh( draw.myMeshId );

            VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
            VkDeviceSize offsets[]       = { 0 };
            vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
            vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

            const uint32_t dynamicOffset = draw.myInstanceDataOffset;
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &objectDescriptor, 1, &dynamicOffset );

            aCore.myFrameStats.myDrawCalls++;
            vkCmdDrawIndexed( aCommandBuffer, mesh.myIndexCount, 1, 0, 0, 0 );
        }
    }
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

void Destroy( Vk_Core& aCore ) {
    aCore.myShadowMapState.myInitialized = false;
    aCore.myShadowMapState.myDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void CmdBarrierForDeferredRead( Vk_Core& aCore, VkCommandBuffer aCommandBuffer ) {
    if ( !aCore.myShadowMapState.myInitialized || !aCore.myLightingSettings.myShadowsEnabled ) {
        return;
    }

    Vk_ShadowMapState& state = aCore.myShadowMapState;
    if ( state.myDepthLayout == VK_IMAGE_LAYOUT_UNDEFINED ) {
        // Shadow pass did not run this frame — still need a valid layout for sampler validation.
        CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
        return;
    }

    if ( state.myDepthLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
        const VkImageMemoryBarrier barrier =
            MakeShadowDepthBarrier( state.myDepth.Image(), state.myDepthLayout, state.myDepthLayout, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT );
        vkCmdPipelineBarrier( aCommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &barrier );
        return;
    }

    CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT );
}

void Init( Vk_Core& aCore ) {
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

void RecordDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aCasterPass, bool aEmitDebugLabels ) {
    if ( !aCore.myShadowMapState.myInitialized || !aCore.myLightingSettings.myShadowsEnabled ) {
        return;
    }

    const glm::vec3  sunDir                = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    const Gfx_Bounds sceneBounds           = aCore.GetShadowCasterBounds();
    aCore.myShadowMapState.myLightViewProj = Gfx_LightingMath::Gfx_ComputeKhronosDirectionalShadowMatrixFromScene( sunDir, sceneBounds );

    Vk_ShadowMapState& state = aCore.myShadowMapState;

    CmdTransitionShadowDepth( state, aCommandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT );

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass        = state.myRenderPass;
    beginInfo.framebuffer       = state.myFramebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    VkClearValue clear{};
    clear.depthStencil        = { 0.0f, 0 };
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues    = &clear;

    VkViewport viewport{};
    viewport.width    = static_cast< float >( Vk_ShadowMapState::kMapSize );
    viewport.height   = static_cast< float >( Vk_ShadowMapState::kMapSize );
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{ { 0, 0 }, { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize } };

    if ( aEmitDebugLabels ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=ShadowMap" );
    }

    vkCmdBeginRenderPass( aCommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myPipeline );
    vkCmdSetViewport( aCommandBuffer, 0, 1, &viewport );
    vkCmdSetScissor( aCommandBuffer, 0, 1, &scissor );
    vkCmdSetDepthBias( aCommandBuffer, kKhronosDepthBiasConstant, 0.0f, kKhronosDepthBiasSlope );

    if ( !aCasterPass.myDraws.empty() ) {
        RecordShadowDraws( aCore, aCommandBuffer, aCasterPass, aEmitDebugLabels );
    }

    vkCmdEndRenderPass( aCommandBuffer );
    state.myDepthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    if ( aEmitDebugLabels ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    Vk_FrameUniformUploader::UpdateLightingGlobals( aCore, aCore.myFrameCtx.myCurrentFrame, state.myLightViewProj );
}

}  // namespace Vk_ShadowMapPass

// Module: Vk_ShadowMapPass — directional shadow depth map (S5 v0).
#include "Vk_ShadowMapPass.h"

#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Core.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"

#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <stdexcept>

namespace {

constexpr char kShadowVertSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapVert.spv";
constexpr char kShadowFragSpv[] = "VulkanDesktop/Shader_Generated/ShadowMapFrag.spv";

struct ShadowPushConstants {
    alignas( 16 ) glm::mat4 lightViewProj;
};

static_assert( sizeof( ShadowPushConstants ) == 64, "ShadowPushConstants must match ShadowMap.vert push block" );

VkDeviceSize IndirectByteOffset( uint32_t aDrawSlot ) {
    return static_cast< VkDeviceSize >( aDrawSlot ) * sizeof( VkDrawIndexedIndirectCommand );
}

uint32_t ComputeIndirectDrawSlot( bool aUseGpuCullIndirect, uint32_t aViewBaseIndex, uint32_t aPassOffset, uint32_t aDrawIndexInPass, uint32_t aEntitySlot ) {
    return aUseGpuCullIndirect ? Gfx_ComputeEntityIndirectSlot( aViewBaseIndex, aEntitySlot ) : Gfx_ComputeDrawBufferSlot( aViewBaseIndex, aPassOffset, aDrawIndexInPass );
}

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
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
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
    samplerInfo.compareOp        = VK_COMPARE_OP_LESS;
    samplerInfo.anisotropyEnable = VK_FALSE;
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myCompareSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_ShadowMapPass: failed to create compare sampler" );
    }

    const std::string vertPath   = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowVertSpv );
    const std::string fragPath   = UtilLoader::ResolvePath( aCore.EngineConfig(), kShadowFragSpv );
    VkShaderModule    vertModule = aCore.CreateShaderModule( vertPath );
    VkShaderModule    fragModule = aCore.CreateShaderModule( fragPath );

    Vk_PipelineBuilder pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo               = VkInit::Pipeline_VertexInputStateCreateInfo();
    pipelineBuilder.myInputAssembly                 = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                      = VkInit::ViewportCreateInfo( { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize } );
    pipelineBuilder.myScissor.offset                = { 0, 0 };
    pipelineBuilder.myScissor.extent                = { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    pipelineBuilder.myRasterizer                    = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT );
    pipelineBuilder.myMultisampling                 = VkInit::Pipeline_MultisampleCreateInfo( VK_SAMPLE_COUNT_1_BIT );
    pipelineBuilder.myDepthStencil                  = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthTestEnable  = VK_TRUE;
    pipelineBuilder.myDepthStencil.depthWriteEnable = VK_TRUE;
    pipelineBuilder.myColorBlendAttachment          = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                = state.myPipelineLayout;
    pipelineBuilder.SetDefaultDynamicStates();
    state.myPipeline = pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, state.myRenderPass, aCore.myDeviceCtx.myPipelineCache, nullptr );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );

    const VkDevice         device      = aCore.myDeviceCtx.myDevice;
    const VmaAllocator     allocator   = aCore.myDeviceCtx.myAllocator;
    const VkRenderPass     renderPass  = state.myRenderPass;
    const VkFramebuffer    framebuffer = state.myFramebuffer;
    const VkPipeline       pipeline    = state.myPipeline;
    const VkPipelineLayout layout      = state.myPipelineLayout;
    const VkSampler        sampler     = state.myCompareSampler;
    const VkImageView      depthView   = state.myDepth.ImageView();
    const VkImage          depthImage  = state.myDepth.Image();
    const VmaAllocation    depthAlloc  = state.myDepth.Allocation();
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, allocator, renderPass, framebuffer, pipeline, layout, sampler, depthView, depthImage, depthAlloc ]() {
        vkDestroySampler( device, sampler, nullptr );
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

void RecordShadowDraws( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aPass, uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer,
                        bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels ) {
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
            const uint32_t slot = ComputeIndirectDrawSlot( aUseGpuCullIndirect, aDrawBufferBaseIndex, aPass.myDrawBufferPassOffset, absoluteDrawIndex, draw.myEntityIndex );

            if ( aEmitDebugLabels ) {
                aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=ShadowMap" );
            }

            VkBuffer     vertexBuffers[] = { mesh.myVertexBuffer.myBuffer };
            VkDeviceSize offsets[]       = { 0 };
            vkCmdBindVertexBuffers( aCommandBuffer, 0, 1, vertexBuffers, offsets );
            vkCmdBindIndexBuffer( aCommandBuffer, mesh.myIndexBuffer.myBuffer, 0, VK_INDEX_TYPE_UINT32 );

            const uint32_t dynamicOffset = draw.myInstanceDataOffset;
            vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, VkDescriptorPolicy::kSetObject, 1, &objectDescriptor, 1, &dynamicOffset );

            aCore.myFrameStats.myDrawCalls++;
            if ( aUseLegacyDirectDraw ) {
                vkCmdDrawIndexed( aCommandBuffer, mesh.myIndexCount, 1, 0, 0, 0 );
            }
            else {
                vkCmdDrawIndexedIndirect( aCommandBuffer, aIndirectBuffer, IndirectByteOffset( slot ), 1, sizeof( VkDrawIndexedIndirectCommand ) );
            }

            if ( aEmitDebugLabels ) {
                aCore.CmdEndDebugLabel( aCommandBuffer );
            }
        }
    }
}

}  // namespace

namespace Vk_ShadowMapPass {

void Destroy( Vk_Core& aCore ) {
    aCore.myShadowMapState.myInitialized = false;
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
}

void RecordDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, const Gfx_PassDrawPacket& aOpaquePass, uint32_t aDrawBufferBaseIndex, VkBuffer aIndirectBuffer,
                 bool aUseGpuCullIndirect, bool aUseLegacyDirectDraw, bool aEmitDebugLabels ) {
    if ( !aCore.myShadowMapState.myInitialized || !aCore.myLightingSettings.myShadowsEnabled ) {
        return;
    }
    if ( aOpaquePass.myDraws.empty() ) {
        return;
    }

    const glm::vec3 sunDir                 = glm::normalize( glm::vec3( aCore.myEnvironmentData.mySunlightDirection ) );
    aCore.myShadowMapState.myLightViewProj = Gfx_LightingMath::ComputeDirectionalShadowMatrix( sunDir, aCore.myCamera, static_cast< float >( Vk_ShadowMapState::kMapSize ) );

    Vk_ShadowMapState& state = aCore.myShadowMapState;

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass        = state.myRenderPass;
    beginInfo.framebuffer       = state.myFramebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize };
    VkClearValue clear{};
    clear.depthStencil        = { 1.0f, 0 };
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues    = &clear;

    VkViewport viewport{};
    viewport.width    = static_cast< float >( Vk_ShadowMapState::kMapSize );
    viewport.height   = static_cast< float >( Vk_ShadowMapState::kMapSize );
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{ { 0, 0 }, { Vk_ShadowMapState::kMapSize, Vk_ShadowMapState::kMapSize } };

    vkCmdBeginRenderPass( aCommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myPipeline );
    vkCmdSetViewport( aCommandBuffer, 0, 1, &viewport );
    vkCmdSetScissor( aCommandBuffer, 0, 1, &scissor );

    RecordShadowDraws( aCore, aCommandBuffer, aOpaquePass, aDrawBufferBaseIndex, aIndirectBuffer, aUseGpuCullIndirect, aUseLegacyDirectDraw, aEmitDebugLabels );

    vkCmdEndRenderPass( aCommandBuffer );
}

}  // namespace Vk_ShadowMapPass

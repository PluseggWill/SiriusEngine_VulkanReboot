// Module: Vk_DeferredLightingPass — FG v0 clustered deferred resolve to swapchain.
// Set 0: G-buffer samplers + ClusterBuild SSBOs (lights, per-frame cluster lists).
#include "Vk_DeferredLightingPass.h"

#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"
#include "Vk_Camera.h"
#include "Vk_Core.h"
#include "Vk_Initializer.h"
#include "Vk_Pipeline.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr char kDeferredVertSpv[] = "VulkanDesktop/Shader_Generated/DeferredLightingVert.spv";
constexpr char kDeferredFragSpv[] = "VulkanDesktop/Shader_Generated/DeferredLightingFrag.spv";

VkPipeline BuildFullscreenPipeline( Vk_Core& aCore, VkRenderPass aSwapchainRenderPass, VkPipelineLayout aLayout, const std::string& aVertPath, const std::string& aFragPath ) {
    VkShaderModule vertModule = aCore.CreateShaderModule( aVertPath );
    VkShaderModule fragModule = aCore.CreateShaderModule( aFragPath );

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = VkInit::Pipeline_VertexInputStateCreateInfo();
    Vk_PipelineBuilder                   pipelineBuilder;
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main" ) );
    pipelineBuilder.myShaderStages.push_back( VkInit::Pipeline_ShaderStageCreateInfo( VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main" ) );
    pipelineBuilder.myVertexInputInfo               = vertexInputInfo;
    pipelineBuilder.myInputAssembly                 = VkInit::Pipeline_InputAssemblyCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST );
    pipelineBuilder.myViewport                      = VkInit::ViewportCreateInfo( aCore.mySwapchainCtx.mySwapChainExtent );
    pipelineBuilder.myScissor.offset                = { 0, 0 };
    pipelineBuilder.myScissor.extent                = aCore.mySwapchainCtx.mySwapChainExtent;
    pipelineBuilder.myRasterizer                    = VkInit::Pipeline_RasterizationCreateInfo( VK_POLYGON_MODE_FILL );
    pipelineBuilder.myMultisampling                 = VkInit::Pipeline_MultisampleCreateInfo( aCore.mySwapchainCtx.myMSAASamples );
    pipelineBuilder.myDepthStencil                  = VkInit::Pipeline_DepthStencilCreateInfo();
    pipelineBuilder.myDepthStencil.depthWriteEnable = VK_FALSE;
    pipelineBuilder.myDepthStencil.depthTestEnable  = VK_FALSE;
    pipelineBuilder.myColorBlendAttachment          = VkInit::Pipeline_ColorBlendAttachment( VK_FALSE );
    pipelineBuilder.myPipelineLayout                = aLayout;
    pipelineBuilder.SetDefaultDynamicStates();

    VkPipeline pipeline = pipelineBuilder.BuildPipeline( aCore.myDeviceCtx.myDevice, aSwapchainRenderPass, aCore.myDeviceCtx.myPipelineCache, nullptr );

    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, vertModule, nullptr );
    vkDestroyShaderModule( aCore.myDeviceCtx.myDevice, fragModule, nullptr );
    return pipeline;
}

// Per in-flight frame: cluster-list SSBO differs; G-buffer views refresh on resize.
void UpdateDescriptorSet( Vk_Core& aCore, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.sampler     = state.myGBufferSampler;
    albedoInfo.imageView   = aCore.myGBufferState.myAlbedo.ImageView();
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler     = state.myGBufferSampler;
    normalInfo.imageView   = aCore.myGBufferState.myNormalRoughness.ImageView();
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo lightsInfo{};
    lightsInfo.buffer = aCore.myClusterBuildState.myLightsBuffer.myBuffer;
    lightsInfo.offset = 0;
    lightsInfo.range  = sizeof( Gfx_ClusterLighting::GpuClusterLight ) * Gfx_ClusterLighting::kMaxLights;

    VkDescriptorBufferInfo listsInfo{};
    listsInfo.buffer = aCore.myClusterBuildState.myClusterListBuffers[ aFrameIndex ].myBuffer;
    listsInfo.offset = 0;
    listsInfo.range  = VK_WHOLE_SIZE;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler     = state.myGBufferSampler;
    depthInfo.imageView   = aCore.myGBufferState.myDepth.ImageView();
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array< VkWriteDescriptorSet, 5 > writes = {
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &albedoInfo, 0, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &normalInfo, 1, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &lightsInfo, 2, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &listsInfo, 3, 1 ),
        VkInit::DescriptorSetWriteCreateInfo( state.myDescriptorSets[ aFrameIndex ], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthInfo, 4, 1 ),
    };
    vkUpdateDescriptorSets( aCore.myDeviceCtx.myDevice, static_cast< uint32_t >( writes.size() ), writes.data(), 0, nullptr );
}

void CreatePipelineResources( Vk_Core& aCore ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;

    const std::array< VkDescriptorSetLayoutBinding, 5 > bindings = {
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ),
        VkInit::DescriptorSetLayoutBindingCreateInfo( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4 ),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast< uint32_t >( bindings.size() );
    layoutInfo.pBindings    = bindings.data();
    if ( vkCreateDescriptorSetLayout( aCore.myDeviceCtx.myDevice, &layoutInfo, nullptr, &state.mySetLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create descriptor set layout" );
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof( Gfx_ClusterLighting::Gfx_DeferredLightingPushConstants );

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::Pipeline_LayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount             = 1;
    pipelineLayoutInfo.pSetLayouts                = &state.mySetLayout;
    pipelineLayoutInfo.pushConstantRangeCount     = 1;
    pipelineLayoutInfo.pPushConstantRanges        = &pushRange;
    if ( vkCreatePipelineLayout( aCore.myDeviceCtx.myDevice, &pipelineLayoutInfo, nullptr, &state.myPipelineLayout ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create pipeline layout" );
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    if ( vkCreateSampler( aCore.myDeviceCtx.myDevice, &samplerInfo, nullptr, &state.myGBufferSampler ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create sampler" );
    }

    std::array< VkDescriptorPoolSize, 2 > poolSizes = {
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kDeferredLightingFramesInFlight * 3 },
        VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kDeferredLightingFramesInFlight * 2 },
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = kDeferredLightingFramesInFlight;
    if ( vkCreateDescriptorPool( aCore.myDeviceCtx.myDevice, &poolInfo, nullptr, &state.myDescriptorPool ) != VK_SUCCESS ) {
        throw std::runtime_error( "Vk_DeferredLightingPass: failed to create descriptor pool" );
    }

    for ( uint32_t i = 0; i < kDeferredLightingFramesInFlight; ++i ) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = state.myDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &state.mySetLayout;
        if ( vkAllocateDescriptorSets( aCore.myDeviceCtx.myDevice, &allocInfo, &state.myDescriptorSets[ i ] ) != VK_SUCCESS ) {
            throw std::runtime_error( "Vk_DeferredLightingPass: failed to allocate descriptor set" );
        }
        UpdateDescriptorSet( aCore, i );
    }

    const std::string vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredVertSpv );
    const std::string fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredFragSpv );
    state.myPipeline           = BuildFullscreenPipeline( aCore, aCore.mySwapchainCtx.myRenderPass, state.myPipelineLayout, vertPath, fragPath );

    const VkDevice              device         = aCore.myDeviceCtx.myDevice;
    const VkPipeline            pipeline       = state.myPipeline;
    const VkPipelineLayout      pipelineLayout = state.myPipelineLayout;
    const VkDescriptorSetLayout setLayout      = state.mySetLayout;
    const VkDescriptorPool      descriptorPool = state.myDescriptorPool;
    const VkSampler             sampler        = state.myGBufferSampler;
    aCore.myDeviceCtx.myDeletionQueue.pushFunction( [ device, pipeline, pipelineLayout, setLayout, descriptorPool, sampler ]() {
        if ( pipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( device, pipeline, nullptr );
        }
        if ( pipelineLayout != VK_NULL_HANDLE ) {
            vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
        }
        if ( setLayout != VK_NULL_HANDLE ) {
            vkDestroyDescriptorSetLayout( device, setLayout, nullptr );
        }
        if ( descriptorPool != VK_NULL_HANDLE ) {
            vkDestroyDescriptorPool( device, descriptorPool, nullptr );
        }
        if ( sampler != VK_NULL_HANDLE ) {
            vkDestroySampler( device, sampler, nullptr );
        }
    } );

    UtilLogger::Info( "PIPELINE", "DeferredLighting graphics pipeline created." );
}

Gfx_ClusterLighting::Gfx_DeferredLightingPushConstants BuildPushConstants( const Vk_Core& aCore ) {
    Gfx_ClusterLighting::Gfx_DeferredLightingPushConstants push{};
    const uint32_t                                         width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t                                         height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    push.tilesX                                                   = Gfx_ClusterLighting::TilesForExtent( width );
    push.tilesY                                                   = Gfx_ClusterLighting::TilesForExtent( height );
    push.tileSize                                                 = Gfx_ClusterLighting::kTileSize;
    push.depthSlice                                               = 0;
    std::memcpy( push.ambientColor, glm::value_ptr( aCore.myEnvironmentData.myAmbientColor ), sizeof( float ) * 4 );
    std::memcpy( push.viewWorldPos, glm::value_ptr( aCore.myEnvironmentData.myViewWorldPos ), sizeof( float ) * 4 );
    push.specularStrength = aCore.myEnvironmentData.myFogDistance.x;
    push.shininess        = glm::max( aCore.myEnvironmentData.myFogDistance.y, 1.0f );

    const glm::mat4 invViewProj = glm::inverse( aCore.myCamera.myProj * aCore.myCamera.myView );
    std::memcpy( push.invViewProj, glm::value_ptr( invViewProj ), sizeof( push.invViewProj ) );
    return push;
}

}  // namespace

namespace Vk_DeferredLightingPass {

void Destroy( Vk_Core& aCore ) {
    if ( !aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    aCore.myDeferredLightingState.myDescriptorSets = {};
    aCore.myDeferredLightingState.myInitialized    = false;
}

void RecreateForExtent( Vk_Core& aCore ) {
    if ( !aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    if ( aCore.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myDeviceCtx.myDevice );
        if ( aCore.myDeferredLightingState.myPipeline != VK_NULL_HANDLE ) {
            vkDestroyPipeline( aCore.myDeviceCtx.myDevice, aCore.myDeferredLightingState.myPipeline, nullptr );
            aCore.myDeferredLightingState.myPipeline = VK_NULL_HANDLE;
        }
    }

    for ( uint32_t i = 0; i < kDeferredLightingFramesInFlight; ++i ) {
        UpdateDescriptorSet( aCore, i );
    }

    const std::string vertPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredVertSpv );
    const std::string fragPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kDeferredFragSpv );
    aCore.myDeferredLightingState.myPipeline =
        BuildFullscreenPipeline( aCore, aCore.mySwapchainCtx.myRenderPass, aCore.myDeferredLightingState.myPipelineLayout, vertPath, fragPath );
}

void Init( Vk_Core& aCore ) {
    if ( aCore.myDeferredLightingState.myInitialized ) {
        return;
    }
    if ( !aCore.myGBufferState.myInitialized || !aCore.myClusterBuildState.myInitialized ) {
        throw std::runtime_error( "Vk_DeferredLightingPass::Init requires GBuffer and ClusterBuild" );
    }
    UtilLogger::Info( "FG", "Vk_DeferredLightingPass::Init." );
    CreatePipelineResources( aCore );
    aCore.myDeferredLightingState.myInitialized = true;
}

void RecordDraw( Vk_Core& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_DeferredLightingState& state = aCore.myDeferredLightingState;
    if ( !state.myInitialized || aFrameIndex >= kDeferredLightingFramesInFlight ) {
        return;
    }

    static bool sDrawLoggedOnce = false;

    const Gfx_ClusterLighting::Gfx_DeferredLightingPushConstants push = BuildPushConstants( aCore );

    vkCmdBindPipeline( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myPipeline );
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state.myPipelineLayout, 0, 1, &state.myDescriptorSets[ aFrameIndex ], 0, nullptr );
    vkCmdPushConstants( aCommandBuffer, state.myPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdBeginDebugLabel( aCommandBuffer, "Pass=DeferredLighting" );
    }
    vkCmdDraw( aCommandBuffer, 3, 1, 0, 0 );
    if ( aCore.AreCommandDebugLabelsEnabled() ) {
        aCore.CmdEndDebugLabel( aCommandBuffer );
    }

    if ( !sDrawLoggedOnce ) {
        UtilLogger::Info( "FG", "DeferredLighting resolve: tiles=" + std::to_string( push.tilesX ) + "x" + std::to_string( push.tilesY ) + " (depth slice 0 stub)" );
        sDrawLoggedOnce = true;
    }
}

}  // namespace Vk_DeferredLightingPass

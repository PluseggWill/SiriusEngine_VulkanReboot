#include "Vk_FrameGraph.h"

#include "../Gfx/Gfx_AoSettings.h"
#include "../Gfx/Gfx_FramePacketValidation.h"
#include "../Gfx/Gfx_LightingMath.h"
#include "../Util/Util_Logger.h"
#include "Vk_AoPass.h"
#include "Vk_ClusterBuildPass.h"
#include "Vk_DeferredLightingPass.h"
#include "Vk_DepthPyramidPass.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_FgBarrierCompiler.h"
#include "Vk_FrameUniformUploader.h"
#include "Vk_GBufferPass.h"
#include "Vk_PostProcessPass.h"
#include "Vk_Renderer.h"
#include "Vk_ScenePasses.h"
#include "Vk_ShadowAoSoftPass.h"
#include "Vk_SsrPass.h"

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <stdexcept>
#include <vector>

namespace {
struct FrameGraphNode {
    Vk_FrameGraphPassId                                  myId;
    std::vector< Vk_FrameGraphPassId >                   myDependencies;
    std::function< bool( const Vk_FrameGraphContext& ) > myIsEnabled;
    std::function< void( Vk_FrameGraphContext& ) >       myRecord;
};

void BindHybridSceneDescriptors( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, VkPipelineLayout aFrameBindLayout, VkDescriptorSet aFrameDescriptor, bool aBindless ) {
    vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aFrameBindLayout, VkDescriptorPolicy::kSetFrame, 1, &aFrameDescriptor, 0, nullptr );
    if ( aBindless ) {
        vkCmdBindDescriptorSets( aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aCore.mySceneGpuCtx.myBindlessPipelineLayout, VkDescriptorPolicy::kSetMaterial, 1,
                                 &aCore.mySceneGpuCtx.myBindlessDescriptorSet, 0, nullptr );
        aCore.myFrameStats.myMaterialSetBinds++;
    }
}

std::vector< Vk_FrameGraphPassId > TopologicalSort( const std::vector< FrameGraphNode >& aNodes ) {
    const size_t                         nodeCount = aNodes.size();
    std::vector< int32_t >               inDegree( nodeCount, 0 );
    std::vector< std::vector< size_t > > adjacency( nodeCount );

    auto findIndex = [ & ]( Vk_FrameGraphPassId aId ) -> size_t {
        for ( size_t i = 0; i < nodeCount; ++i ) {
            if ( aNodes[ i ].myId == aId ) {
                return i;
            }
        }
        throw std::runtime_error( "Vk_FrameGraph: unknown pass id in dependency graph" );
    };

    for ( size_t i = 0; i < nodeCount; ++i ) {
        for ( Vk_FrameGraphPassId dep : aNodes[ i ].myDependencies ) {
            const size_t depIndex = findIndex( dep );
            adjacency[ depIndex ].push_back( i );
            ++inDegree[ i ];
        }
    }

    std::vector< size_t > queue;
    for ( size_t i = 0; i < nodeCount; ++i ) {
        if ( inDegree[ i ] == 0 ) {
            queue.push_back( i );
        }
    }

    std::vector< Vk_FrameGraphPassId > sorted;
    size_t                             head = 0;
    while ( head < queue.size() ) {
        const size_t index = queue[ head++ ];
        sorted.push_back( aNodes[ index ].myId );
        for ( size_t child : adjacency[ index ] ) {
            if ( --inDegree[ child ] == 0 ) {
                queue.push_back( child );
            }
        }
    }

    if ( sorted.size() != nodeCount ) {
        throw std::runtime_error( "Vk_FrameGraph: cycle detected in pass graph" );
    }
    return sorted;
}

bool IsShadowPassEnabled( const Vk_FrameGraphContext& aCtx ) {
    const glm::vec3 sunDir = glm::normalize( glm::vec3( aCtx.myCore->myEnvironmentData.mySunlightDirection ) );
    return aCtx.myCore->myShadowMapState.myInitialized && Gfx_LightingMath::Gfx_ShouldCompareDirectionalShadows( aCtx.myCore->myLightingSettings.myShadowsEnabled, sunDir );
}

bool IsDepthPyramidEnabled( const Vk_FrameGraphContext& aCtx ) {
    return aCtx.myCore->myDepthPyramidState.myInitialized;
}
bool IsSsrPassEnabled( const Vk_FrameGraphContext& aCtx ) {
    return aCtx.myCore->mySsrState.myInitialized && aCtx.myCore->myDepthPyramidState.myInitialized;
}
bool IsAoPassEnabled( const Vk_FrameGraphContext& aCtx ) {
    return aCtx.myCore->myAoSettings.myEnabled && aCtx.myCore->myAoState.myInitialized && IsDepthPyramidEnabled( aCtx );
}
bool IsShadowAoSoftEnabled( const Vk_FrameGraphContext& aCtx ) {
    return aCtx.myCore->myShadowAoSoftState.myInitialized && aCtx.myCore->myAoSettings.myContactSoftEnabled;
}
bool IsDdgiProbeUpdateEnabled( const Vk_FrameGraphContext& aCtx ) {
    return aCtx.myCore->myDeferredLightingState.myInitialized && aCtx.myCore->myLightingSettings.myDdgiEnabled;
}

void BuildHybridDeferredNodes( std::vector< FrameGraphNode >& aOutNodes ) {
    aOutNodes.clear();
    aOutNodes.reserve( static_cast< size_t >( Vk_FrameGraphPassId::Count ) );

    aOutNodes.push_back( FrameGraphNode{ Vk_FrameGraphPassId::Shadow, {}, IsShadowPassEnabled, []( Vk_FrameGraphContext& aCtx ) {
                                            constexpr uint32_t           viewIndex       = 0;
                                            const Gfx_FrameRenderPacket* packet          = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
                                            const bool                   emitDebugLabels = aCtx.myCore->AreCommandDebugLabelsEnabled();
                                            if ( packet != nullptr && !packet->myShadowCasterPass.myDraws.empty() ) {
                                                Vk_ShadowMapPass::RecordDraw( *aCtx.myCore, aCtx.myCommandBuffer, packet->myShadowCasterPass, emitDebugLabels );
                                            }
                                            else {
                                                Vk_ShadowMapPass::RecordDraw( *aCtx.myCore, aCtx.myCommandBuffer, Gfx_PassDrawPacket{}, emitDebugLabels );
                                            }
                                        } } );

    aOutNodes.push_back(
        FrameGraphNode{ Vk_FrameGraphPassId::GBuffer,
                        { Vk_FrameGraphPassId::Shadow },
                        []( const Vk_FrameGraphContext& ) { return true; },
                        []( Vk_FrameGraphContext& aCtx ) {
                            Vk_Renderer&                 aCore            = *aCtx.myCore;
                            const Vk_RenderMaterialPath  materialPath     = aCore.myRhi.myDeviceCtx.myMaterialPath;
                            const bool                   bindless         = materialPath == Vk_RenderMaterialPath::Bindless;
                            const VkPipelineLayout       frameBindLayout  = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;
                            const VkPipeline             gbufferPipeline  = bindless ? aCore.myGBufferState.myGBufferPipelineBindless : aCore.myGBufferState.myGBufferPipeline;
                            const bool                   legacyDirectDraw = aCtx.myToggles->myLegacyDirectDraw;
                            const bool                   gpuCullRecord    = aCtx.myToggles->myGpuCullEnabled && !legacyDirectDraw;
                            const bool                   emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();
                            Vk_FrameData&                frame            = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
                            const VkBuffer               indirectBuffer   = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;
                            constexpr uint32_t           viewIndex        = 0;
                            const Gfx_FrameRenderPacket* packet           = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
                            if ( !IsShadowPassEnabled( aCtx ) ) {
                                Vk_FrameUniformUploader::UpdateLightingGlobalsFromScene( aCore, aCore.myFrameCtx.myCurrentFrame );
                            }
                            VkRenderPassBeginInfo gbufferBegin{};
                            gbufferBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                            gbufferBegin.renderPass        = aCore.myGBufferState.myRenderPass;
                            gbufferBegin.framebuffer       = aCore.myGBufferState.myFramebuffer;
                            gbufferBegin.renderArea.offset = { 0, 0 };
                            gbufferBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
                            std::array< VkClearValue, 5 > gbufferClears{};
                            gbufferClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
                            gbufferClears[ 1 ].color        = { { 0.0f, 0.0f, 1.0f, 0.5f } };
                            gbufferClears[ 2 ].color        = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                            gbufferClears[ 3 ].color        = { { 0.0f, 0.0f, 0.0f, 0.0f } };
                            gbufferClears[ 4 ].depthStencil = { 1.0f, 0 };
                            gbufferBegin.clearValueCount    = static_cast< uint32_t >( gbufferClears.size() );
                            gbufferBegin.pClearValues       = gbufferClears.data();
                            vkCmdBeginRenderPass( aCtx.myCommandBuffer, &gbufferBegin, VK_SUBPASS_CONTENTS_INLINE );
                            if ( packet != nullptr && aCtx.myViewports != nullptr && aCtx.myScissors != nullptr && aCtx.myFrameDescriptors != nullptr ) {
                                aCore.SetGraphicsDynamicState( aCtx.myCommandBuffer, ( *aCtx.myViewports )[ viewIndex ], ( *aCtx.myScissors )[ viewIndex ] );
                                BindHybridSceneDescriptors( aCore, aCtx.myCommandBuffer, frameBindLayout, ( *aCtx.myFrameDescriptors )[ viewIndex ], bindless );
                                if ( gbufferPipeline != VK_NULL_HANDLE && Gfx_FramePacketValidation::ValidateFramePacket( *packet ) && !aCtx.myToggles->mySkipOpaquePass ) {
                                    if ( emitDebugLabels )
                                        aCore.CmdBeginDebugLabel( aCtx.myCommandBuffer, "Pass=GBufferOpaque" );
                                    Vk_ScenePasses::RecordOpaquePacketDraws( aCore, aCtx.myCommandBuffer, packet->myOpaquePass, packet->myDrawBufferBaseIndex, indirectBuffer,
                                                                             gpuCullRecord, legacyDirectDraw, emitDebugLabels, gbufferPipeline );
                                    if ( emitDebugLabels )
                                        aCore.CmdEndDebugLabel( aCtx.myCommandBuffer );
                                }
                            }
                            vkCmdEndRenderPass( aCtx.myCommandBuffer );
                        } } );

    aOutNodes.push_back(
        FrameGraphNode{ Vk_FrameGraphPassId::ClusterBuild,
                        { Vk_FrameGraphPassId::GBuffer },
                        []( const Vk_FrameGraphContext& ) { return true; },
                        []( Vk_FrameGraphContext& aCtx ) { Vk_ClusterBuildPass::RecordDispatch( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex ); } } );

    aOutNodes.push_back( FrameGraphNode{ Vk_FrameGraphPassId::DepthPyramid, { Vk_FrameGraphPassId::ClusterBuild }, IsDepthPyramidEnabled, []( Vk_FrameGraphContext& aCtx ) {
                                            Vk_FgBarrierCompiler::CmdBarrierGBufferColorsForDeferredRead( *aCtx.myCore, aCtx.myCommandBuffer );
                                            Vk_FgBarrierCompiler::CmdBarrierGBufferDepthForShaderRead( *aCtx.myCore, aCtx.myCommandBuffer );
                                            Vk_DepthPyramidPass::RecordBuild( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
                                        } } );
    aOutNodes.push_back( FrameGraphNode{ Vk_FrameGraphPassId::SSR, { Vk_FrameGraphPassId::DepthPyramid }, IsSsrPassEnabled, []( Vk_FrameGraphContext& aCtx ) {
                                            Vk_FgBarrierCompiler::CmdBarrierGBufferColorsForDeferredRead( *aCtx.myCore, aCtx.myCommandBuffer );
                                            Vk_FgBarrierCompiler::CmdBarrierGBufferDepthForShaderRead( *aCtx.myCore, aCtx.myCommandBuffer );
                                            Vk_SsrPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
                                        } } );
    aOutNodes.push_back( FrameGraphNode{ Vk_FrameGraphPassId::SSAO, { Vk_FrameGraphPassId::DepthPyramid }, IsAoPassEnabled, []( Vk_FrameGraphContext& aCtx ) {
                                            Vk_AoPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
                                        } } );
    aOutNodes.push_back(
        FrameGraphNode{ Vk_FrameGraphPassId::DdgiProbeUpdate,
                        { Vk_FrameGraphPassId::ClusterBuild, Vk_FrameGraphPassId::DepthPyramid, Vk_FrameGraphPassId::SSAO },
                        IsDdgiProbeUpdateEnabled,
                        []( Vk_FrameGraphContext& aCtx ) { Vk_DeferredLightingPass::RecordDdgiProbeUpdate( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex ); } } );
    aOutNodes.push_back( FrameGraphNode{
        Vk_FrameGraphPassId::ShadowAoSoft,
        { Vk_FrameGraphPassId::GBuffer, Vk_FrameGraphPassId::Shadow, Vk_FrameGraphPassId::SSAO, Vk_FrameGraphPassId::DdgiProbeUpdate },
        IsShadowAoSoftEnabled,
        []( Vk_FrameGraphContext& aCtx ) { Vk_ShadowAoSoftPass::RecordCompute( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myFrameIndex, IsAoPassEnabled( aCtx ) ); } } );
    aOutNodes.push_back(
        FrameGraphNode{ Vk_FrameGraphPassId::DeferredTransparent,
                        { Vk_FrameGraphPassId::ClusterBuild, Vk_FrameGraphPassId::SSR, Vk_FrameGraphPassId::SSAO, Vk_FrameGraphPassId::DdgiProbeUpdate,
                          Vk_FrameGraphPassId::Shadow, Vk_FrameGraphPassId::ShadowAoSoft },
                        []( const Vk_FrameGraphContext& ) { return true; },
                        []( Vk_FrameGraphContext& aCtx ) {
                            Vk_Renderer& aCore = *aCtx.myCore;
                            Vk_ShadowMapPass::CmdBarrierForDeferredRead( aCore, aCtx.myCommandBuffer );
                            Vk_FgBarrierCompiler::CmdCopyGBufferDepthToSwapchain( aCore, aCtx.myCommandBuffer );
                            VkRenderPassBeginInfo hybridBegin{};
                            hybridBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                            hybridBegin.renderPass        = aCore.myPostProcessState.myHybridRenderPass;
                            hybridBegin.framebuffer       = aCore.myPostProcessState.myHybridFramebuffer;
                            hybridBegin.renderArea.offset = { 0, 0 };
                            hybridBegin.renderArea.extent = aCore.mySwapchainCtx.mySwapChainExtent;
                            std::array< VkClearValue, 2 > hybridClears{};
                            hybridClears[ 0 ].color        = { { 0.0f, 0.0f, 0.0f, 1.0f } };
                            hybridClears[ 1 ].depthStencil = { 1.0f, 0 };
                            hybridBegin.clearValueCount    = static_cast< uint32_t >( hybridClears.size() );
                            hybridBegin.pClearValues       = hybridClears.data();
                            vkCmdBeginRenderPass( aCtx.myCommandBuffer, &hybridBegin, VK_SUBPASS_CONTENTS_INLINE );
                            const Vk_RenderMaterialPath  materialPath     = aCore.myRhi.myDeviceCtx.myMaterialPath;
                            const bool                   bindless         = materialPath == Vk_RenderMaterialPath::Bindless;
                            const VkPipelineLayout       frameBindLayout  = bindless ? aCore.mySceneGpuCtx.myBindlessPipelineLayout : aCore.mySceneGpuCtx.myPipelineLayout;
                            const bool                   legacyDirectDraw = aCtx.myToggles->myLegacyDirectDraw;
                            const bool                   gpuCullRecord    = aCtx.myToggles->myGpuCullEnabled && !legacyDirectDraw;
                            const bool                   emitDebugLabels  = aCore.AreCommandDebugLabelsEnabled();
                            Vk_FrameData&                frame            = aCore.myFrameCtx.myFrameDatas[ aCore.myFrameCtx.myCurrentFrame ];
                            const VkBuffer               indirectBuffer   = gpuCullRecord ? frame.myGpuCullIndirectBuffer.myBuffer : frame.myDrawIndirectBuffer.myBuffer;
                            constexpr uint32_t           viewIndex        = 0;
                            const Gfx_FrameRenderPacket* packet           = viewIndex < aCtx.myViewCount ? &( *aCtx.myViewPackets )[ viewIndex ] : nullptr;
                            if ( aCtx.myViewports != nullptr && aCtx.myScissors != nullptr ) {
                                aCore.SetGraphicsDynamicState( aCtx.myCommandBuffer, ( *aCtx.myViewports )[ viewIndex ], ( *aCtx.myScissors )[ viewIndex ] );
                            }
                            Vk_DeferredLightingPass::RecordDraw( aCore, aCtx.myCommandBuffer, aCtx.myFrameIndex );
                            if ( packet != nullptr && aCtx.myFrameDescriptors != nullptr && Gfx_FramePacketValidation::ValidateFramePacket( *packet )
                                 && !aCtx.myToggles->mySkipTransparentPass && !packet->myTransparentPass.myDraws.empty() ) {
                                BindHybridSceneDescriptors( aCore, aCtx.myCommandBuffer, frameBindLayout, ( *aCtx.myFrameDescriptors )[ viewIndex ], bindless );
                                if ( emitDebugLabels )
                                    aCore.CmdBeginDebugLabel( aCtx.myCommandBuffer, "Pass=ForwardTransparent" );
                                Vk_ScenePasses::RecordTransparentPacketDraws( aCore, aCtx.myCommandBuffer, packet->myTransparentPass, packet->myDrawBufferBaseIndex,
                                                                              indirectBuffer, gpuCullRecord, legacyDirectDraw, emitDebugLabels );
                                if ( emitDebugLabels )
                                    aCore.CmdEndDebugLabel( aCtx.myCommandBuffer );
                            }
                            if ( aCtx.myViewports != nullptr && aCtx.myScissors != nullptr && aCtx.myFrameDescriptors != nullptr && aCtx.myViewPackets != nullptr ) {
                                Vk_ScenePasses::RecordHybridPiPViews( aCore, *aCtx.myToggles, aCtx.myCommandBuffer, *aCtx.myViewports, *aCtx.myScissors,
                                                                      *aCtx.myFrameDescriptors, aCtx.myViewCount, *aCtx.myViewPackets );
                            }
                            vkCmdEndRenderPass( aCtx.myCommandBuffer );
                            Vk_PostProcessPass::MarkSceneColorShaderRead();
                            // Copy lit scene color into SSR history (1-frame lag for temporal hit radiance).
                            Vk_SsrPass::RecordHistoryUpdate( aCore, aCtx.myCommandBuffer );
                        } } );
    aOutNodes.push_back(
        FrameGraphNode{ Vk_FrameGraphPassId::Post,
                        { Vk_FrameGraphPassId::DeferredTransparent },
                        []( const Vk_FrameGraphContext& aCtx ) { return Vk_PostProcessPass::HasHybridResolve( *aCtx.myCore ); },
                        []( Vk_FrameGraphContext& aCtx ) { Vk_PostProcessPass::RecordPost( *aCtx.myCore, aCtx.myCommandBuffer, aCtx.myImageIndex, aCtx.myFrameIndex ); } } );
}
}  // namespace

namespace Vk_FrameGraph {
void Execute( Vk_FrameGraphContext& aCtx ) {
    static bool sChainLoggedOnce = false;
    if ( !sChainLoggedOnce ) {
        UtilLogger::Info(
            "FG",
            "HybridDeferred FG v2: Shadow -> GBuffer -> ClusterBuild -> DepthPyramid -> SSR -> AO -> DDGIProbeUpdate -> ShadowAoSoft -> DeferredTransparent(HDR) -> Post" );
        sChainLoggedOnce = true;
    }
    std::vector< FrameGraphNode > nodes;
    BuildHybridDeferredNodes( nodes );
    const std::vector< Vk_FrameGraphPassId > sorted = TopologicalSort( nodes );
    for ( Vk_FrameGraphPassId passId : sorted ) {
        const auto nodeIt = std::find_if( nodes.begin(), nodes.end(), [ passId ]( const FrameGraphNode& aNode ) { return aNode.myId == passId; } );
        if ( nodeIt == nodes.end() )
            continue;
        if ( nodeIt->myIsEnabled && !nodeIt->myIsEnabled( aCtx ) )
            continue;
        if ( nodeIt->myRecord )
            nodeIt->myRecord( aCtx );
    }
}
}  // namespace Vk_FrameGraph

#include "Gfx_RenderPipeline.h"

#include <stdexcept>
#include <vector>

namespace {

void AddNode( Gfx_FramePlan& aPlan, Gfx_PassId aId, std::initializer_list< Gfx_PassId > aDeps, bool aEnabled ) {
    Gfx_FramePlanNode node{};
    node.myId = aId;
    node.myDependencies.assign( aDeps );
    node.myEnabled = aEnabled;
    aPlan.myNodes.push_back( std::move( node ) );
}

std::vector< Gfx_PassId > TopologicalSort( const std::vector< Gfx_FramePlanNode >& aNodes ) {
    const size_t                         nodeCount = aNodes.size();
    std::vector< int32_t >               inDegree( nodeCount, 0 );
    std::vector< std::vector< size_t > > adjacency( nodeCount );

    auto findIndex = [ & ]( Gfx_PassId aId ) -> size_t {
        for ( size_t i = 0; i < nodeCount; ++i ) {
            if ( aNodes[ i ].myId == aId ) {
                return i;
            }
        }
        throw std::runtime_error( "Gfx_RenderPipeline: unknown pass id in dependency graph" );
    };

    for ( size_t i = 0; i < nodeCount; ++i ) {
        for ( Gfx_PassId dep : aNodes[ i ].myDependencies ) {
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

    std::vector< Gfx_PassId > sorted;
    size_t                    head = 0;
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
        throw std::runtime_error( "Gfx_RenderPipeline: cycle detected in pass graph" );
    }
    return sorted;
}

}  // namespace

namespace Gfx_RenderPipeline {

Gfx_FramePlan BuildHybridDeferred( const Gfx_PipelineEnableFlags& aEnable ) {
    Gfx_FramePlan plan{};
    plan.myNodes.reserve( static_cast< size_t >( Gfx_PassId::Count ) );

    AddNode( plan, Gfx_PassId::Shadow, {}, aEnable.myShadow );
    AddNode( plan, Gfx_PassId::GBuffer, { Gfx_PassId::Shadow }, aEnable.myGBuffer );
    AddNode( plan, Gfx_PassId::ClusterBuild, { Gfx_PassId::GBuffer }, aEnable.myClusterBuild );
    AddNode( plan, Gfx_PassId::DepthPyramid, { Gfx_PassId::ClusterBuild }, aEnable.myDepthPyramid );
    AddNode( plan, Gfx_PassId::SSR, { Gfx_PassId::DepthPyramid }, aEnable.mySsr );
    AddNode( plan, Gfx_PassId::SSAO, { Gfx_PassId::DepthPyramid }, aEnable.myAo );
    AddNode( plan, Gfx_PassId::DdgiProbeUpdate, { Gfx_PassId::ClusterBuild, Gfx_PassId::DepthPyramid, Gfx_PassId::SSAO }, aEnable.myDdgiProbeUpdate );
    AddNode( plan, Gfx_PassId::ShadowAoSoft, { Gfx_PassId::GBuffer, Gfx_PassId::Shadow, Gfx_PassId::SSAO, Gfx_PassId::DdgiProbeUpdate }, aEnable.myShadowAoSoft );
    AddNode( plan, Gfx_PassId::DeferredTransparent,
             { Gfx_PassId::ClusterBuild, Gfx_PassId::SSR, Gfx_PassId::SSAO, Gfx_PassId::DdgiProbeUpdate, Gfx_PassId::Shadow, Gfx_PassId::ShadowAoSoft },
             aEnable.myDeferredTransparent );
    AddNode( plan, Gfx_PassId::Post, { Gfx_PassId::DeferredTransparent }, aEnable.myPost );

    plan.myOrdered = TopologicalSort( plan.myNodes );
    return plan;
}

std::string HybridDeferredChainLabel() {
    return "HybridDeferred FG v2: Shadow -> GBuffer -> ClusterBuild -> DepthPyramid -> SSR -> AO -> DDGIProbeUpdate -> ShadowAoSoft -> DeferredTransparent(HDR) -> Post";
}

}  // namespace Gfx_RenderPipeline

// Module: Vk_ClusterBuildPass — thin loader + Vk mirrors over Gfx_ClusterBuildPass Init/Record.
#include "Vk_ClusterBuildPass.h"

#include "../Gfx/Gfx_ClusterBuildPass.h"
#include "../Gfx/Gfx_ClusterLighting.h"
#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include "Vk_FrameCmd.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert( MAX_FRAMES_IN_FLIGHT <= static_cast< int >( Gfx_ClusterBuildPass::kMaxFramesInFlight ), "Gfx_ClusterBuildPass frame slots must cover MAX_FRAMES_IN_FLIGHT" );

namespace {

constexpr char kClusterBuildShaderPath[] = "VulkanDesktop/Shader_Generated/ClusterBuild.spv";

std::vector< char > LoadSpirvBytes( const std::string& aPath ) {
    std::ifstream file( aPath, std::ios::ate | std::ios::binary );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: failed to open shader " + aPath );
    }
    const size_t        fileSize = static_cast< size_t >( file.tellg() );
    std::vector< char > buffer( fileSize );
    file.seekg( 0 );
    file.read( buffer.data(), static_cast< std::streamsize >( fileSize ) );
    return buffer;
}

void WriteSunLightFromEnvironment( Gpu_ClusterLight& aOut, const Gpu_EnvironmentData& aEnv ) {
    const glm::vec3 sunDir = glm::normalize( glm::vec3( aEnv.mySunlightDirection ) );
    const glm::vec3 sunCol = glm::vec3( aEnv.mySunlightColor ) * aEnv.mySunlightColor.w;
    std::memcpy( aOut.direction, glm::value_ptr( glm::vec4( sunDir, 0.0f ) ), sizeof( float ) * 4 );
    std::memcpy( aOut.color, glm::value_ptr( glm::vec4( sunCol, 1.0f ) ), sizeof( float ) * 4 );
}

void ClearVkMirrors( Vk_ClusterBuildState& aState ) {
    aState.myComputePipeline     = VK_NULL_HANDLE;
    aState.myPipelineLayout      = VK_NULL_HANDLE;
    aState.myDescriptorSetLayout = VK_NULL_HANDLE;
    aState.myDescriptorPool      = VK_NULL_HANDLE;
    aState.myLightsBuffer        = {};
    aState.myLightsMapped        = nullptr;
    for ( Vk_AllocatedBuffer& buf : aState.myClusterListBuffers ) {
        buf = {};
    }
    for ( VkDescriptorSet& set : aState.myDescriptorSets ) {
        set = VK_NULL_HANDLE;
    }
    aState.myClusterCount = 0;
}

void SyncVkMirrors( Vk_Renderer& aCore ) {
    Vk_ClusterBuildState& state = aCore.myClusterBuildState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;
    const auto&           gfx   = state.myGfx;

    state.myComputePipeline     = RhiVulkan::PipelineGetVk( rhi, gfx.myPipeline );
    state.myPipelineLayout      = RhiVulkan::PipelineLayoutGetVk( rhi, gfx.myLayout );
    state.myDescriptorSetLayout = RhiVulkan::DescriptorSetLayoutGetVk( rhi, gfx.mySetLayout );
    state.myDescriptorPool      = RhiVulkan::DescriptorPoolGetVk( rhi, gfx.myPool );

    state.myLightsBuffer.myBuffer = RhiVulkan::BufferGetVk( rhi, gfx.myLightsBuffer );
    state.myLightsMapped          = gfx.myLightsMapped;
    state.myClusterCount          = gfx.myClusterCount;

    for ( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i ) {
        state.myClusterListBuffers[ i ].myBuffer = RhiVulkan::BufferGetVk( rhi, gfx.myClusterListBuffers[ i ] );
        state.myDescriptorSets[ i ]              = RhiVulkan::DescriptorSetGetVk( rhi, gfx.mySets[ i ] );
    }
}

bool CreateOrRefreshLists( Vk_Renderer& aCore ) {
    Vk_ClusterBuildState& state = aCore.myClusterBuildState;
    Rhi_Device&           rhi   = aCore.myGfxRhiDevice;

    const uint32_t width  = aCore.mySwapchainCtx.mySwapChainExtent.width;
    const uint32_t height = aCore.mySwapchainCtx.mySwapChainExtent.height;
    if ( width == 0 || height == 0 ) {
        return false;
    }

    Gfx_ClusterBuildPass::ListsInitDesc listsDesc{};
    listsDesc.myWidth          = width;
    listsDesc.myHeight         = height;
    listsDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    const bool ok              = Gfx_ClusterBuildPass::CreateOrRecreateLists( rhi, listsDesc, state.myGfx );
    if ( !ok ) {
        ClearVkMirrors( state );
        return false;
    }

    SyncVkMirrors( aCore );

    const uint64_t listBytes = static_cast< uint64_t >( state.myClusterCount ) * sizeof( Gpu_ClusterLightList );
    UtilLogger::Info( "CLUSTER", "Cluster lists: extent=" + std::to_string( width ) + "x" + std::to_string( height ) + " clusters=" + std::to_string( state.myClusterCount )
                                     + " bytes/list/frame=" + std::to_string( listBytes ) );
    return true;
}

}  // namespace

namespace Vk_ClusterBuildPass {

void Destroy( Vk_Renderer& aCore ) {
    if ( !aCore.myClusterBuildState.myInitialized ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( aCore.myGfxRhiDevice ) {
        Gfx_ClusterBuildPass::Destroy( aCore.myGfxRhiDevice, aCore.myClusterBuildState.myGfx );
    }
    ClearVkMirrors( aCore.myClusterBuildState );
    aCore.myClusterBuildState.myInitialized = false;
}

void RecreateForExtent( Vk_Renderer& aCore ) {
    if ( !aCore.myClusterBuildState.myInitialized || !aCore.myGfxRhiDevice ) {
        return;
    }
    if ( aCore.myRhi.myDeviceCtx.myDevice != VK_NULL_HANDLE ) {
        vkDeviceWaitIdle( aCore.myRhi.myDeviceCtx.myDevice );
    }
    if ( !CreateOrRefreshLists( aCore ) ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: recreate lists failed" );
    }
}

void Init( Vk_Renderer& aCore ) {
    if ( aCore.myClusterBuildState.myInitialized ) {
        return;
    }
    if ( !aCore.myGfxRhiDevice ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: myGfxRhiDevice required for Gfx Init" );
    }

    UtilLogger::Info( "FG", "Vk_ClusterBuildPass::Init (Gfx create)." );

    const std::string         spvPath = UtilLoader::ResolvePath( aCore.EngineConfig(), kClusterBuildShaderPath );
    const std::vector< char > spirv   = LoadSpirvBytes( spvPath );

    Gfx_ClusterBuildPass::PipelineInitDesc pipeDesc{};
    pipeDesc.mySpirvCode      = spirv.data();
    pipeDesc.mySpirvBytes     = spirv.size();
    pipeDesc.myFramesInFlight = MAX_FRAMES_IN_FLIGHT;
    if ( !Gfx_ClusterBuildPass::CreatePipeline( aCore.myGfxRhiDevice, pipeDesc, aCore.myClusterBuildState.myGfx ) ) {
        throw std::runtime_error( "Vk_ClusterBuildPass: Gfx CreatePipeline failed" );
    }
    if ( !CreateOrRefreshLists( aCore ) ) {
        Gfx_ClusterBuildPass::Destroy( aCore.myGfxRhiDevice, aCore.myClusterBuildState.myGfx );
        ClearVkMirrors( aCore.myClusterBuildState );
        throw std::runtime_error( "Vk_ClusterBuildPass: Gfx CreateOrRecreateLists failed" );
    }

    aCore.myClusterBuildState.myInitialized = true;
    UtilLogger::Info( "PIPELINE", "ClusterBuild compute pipeline created (Gfx)." );
}

void RecordDispatch( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex ) {
    Vk_ClusterBuildState& state = aCore.myClusterBuildState;
    if ( !state.myInitialized || state.myClusterCount == 0 || aFrameIndex >= MAX_FRAMES_IN_FLIGHT || !aCore.myGfxRhiDevice ) {
        return;
    }

    static bool sDispatchLoggedOnce = false;

    if ( state.myLightsMapped != nullptr ) {
        auto* lights = static_cast< Gpu_ClusterLight* >( state.myLightsMapped );
        WriteSunLightFromEnvironment( lights[ 0 ], aCore.myEnvironmentData );
    }

    Vk_FrameCmd::Scope frameCmd = Vk_FrameCmd::Bind( aCore, aCommandBuffer );
    if ( !frameCmd ) {
        return;
    }

    Gfx_ClusterBuildPass::GpuResources gpu = Gfx_ClusterBuildPass::MakeGpuResources( state.myGfx, aFrameIndex );

    Gfx_ClusterBuildPass::RecordInput input{};
    input.clusterCount = state.myClusterCount;
    input.lightCount   = Gfx_ClusterLighting::kMaxLights;
    input.debugLabels  = aCore.AreCommandDebugLabelsEnabled();

    Gfx_ClusterBuildPass::Record( frameCmd.Get(), gpu, input );

    if ( !sDispatchLoggedOnce ) {
        UtilLogger::Info( "CLUSTER",
                          "ClusterBuild dispatch: clusters=" + std::to_string( state.myClusterCount ) + " lights=" + std::to_string( Gfx_ClusterLighting::kMaxLights ) );
        sDispatchLoggedOnce = true;
    }
}

}  // namespace Vk_ClusterBuildPass

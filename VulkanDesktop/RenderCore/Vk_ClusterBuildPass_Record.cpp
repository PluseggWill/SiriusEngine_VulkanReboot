// Module: Vk_ClusterBuildPass record path — thin facade over Gfx_ClusterBuildPass::Record (Rhi).
#include "Vk_ClusterBuildPass.h"

#include "../Gfx/Gfx_ClusterBuildPass.h"
#include "../Gfx/Gfx_ClusterLighting.h"
#include "../RenderContract/Gpu_ClusterLight.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Util/Util_Logger.h"
#include "Vk_Renderer.h"
#include "Vk_RhiBackend.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstring>

namespace {

void WriteSunLightFromEnvironment( Gpu_ClusterLight& aOut, const Gpu_EnvironmentData& aEnv ) {
    const glm::vec3 sunDir = glm::normalize( glm::vec3( aEnv.mySunlightDirection ) );
    const glm::vec3 sunCol = glm::vec3( aEnv.mySunlightColor ) * aEnv.mySunlightColor.w;
    std::memcpy( aOut.direction, glm::value_ptr( glm::vec4( sunDir, 0.0f ) ), sizeof( float ) * 4 );
    std::memcpy( aOut.color, glm::value_ptr( glm::vec4( sunCol, 1.0f ) ), sizeof( float ) * 4 );
}

}  // namespace

namespace Vk_ClusterBuildPass {

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

    Rhi_CommandList cmd = Rhi::DeviceCreateCommandList( aCore.myGfxRhiDevice );
    RhiVulkan::CommandListBindVk( cmd, aCommandBuffer );

    Gfx_ClusterBuildPass::GpuResources gpu{};
    gpu.myPipeline          = RhiVulkan::PipelineAdopt( state.myComputePipeline );
    gpu.myLayout            = RhiVulkan::PipelineLayoutAdopt( state.myPipelineLayout );
    gpu.mySet               = RhiVulkan::DescriptorSetAdopt( state.myDescriptorSets[ aFrameIndex ] );
    gpu.myLightsBuffer      = RhiVulkan::BufferAdopt( state.myLightsBuffer.myBuffer );
    gpu.myClusterListBuffer = RhiVulkan::BufferAdopt( state.myClusterListBuffers[ aFrameIndex ].myBuffer );

    Gfx_ClusterBuildPass::RecordInput input{};
    input.clusterCount = state.myClusterCount;
    input.lightCount   = Gfx_ClusterLighting::kMaxLights;
    input.debugLabels  = aCore.AreCommandDebugLabelsEnabled();

    Gfx_ClusterBuildPass::Record( cmd, gpu, input );

    Rhi::CommandListDestroy( cmd );

    if ( !sDispatchLoggedOnce ) {
        UtilLogger::Info( "CLUSTER",
                          "ClusterBuild dispatch: clusters=" + std::to_string( state.myClusterCount ) + " lights=" + std::to_string( Gfx_ClusterLighting::kMaxLights ) );
        sDispatchLoggedOnce = true;
    }
}

}  // namespace Vk_ClusterBuildPass

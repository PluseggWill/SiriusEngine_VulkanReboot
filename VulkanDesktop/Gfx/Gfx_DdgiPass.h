#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Handles.h"

#include <glm/vec4.hpp>

#include <cstdint>

namespace Gfx_DdgiPass {

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_Texture        myIrradianceAtlas{};
    Rhi_Texture        myVisibilityAtlas{};
};

struct ProbePush {
    glm::uvec4 probeGrid{};
    glm::uvec4 updateInfo{};
    glm::vec4  ambient{};
    glm::vec4  blend{};
    glm::vec4  volumeMin{};
    glm::vec4  volumeMax{};
    glm::vec4  integrateParams{};
};
static_assert( sizeof( ProbePush ) == 112, "ProbePush must match DdgiProbeUpdate.comp" );

struct RecordInput {
    ProbePush myPush{};
    uint32_t  myAtlasWidth  = 0;
    uint32_t  myAtlasHeight = 0;
    bool      myDebugLabels = false;
};

void RecordProbeUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_DdgiPass

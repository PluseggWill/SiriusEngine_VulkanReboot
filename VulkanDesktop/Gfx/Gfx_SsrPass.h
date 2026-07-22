#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>

namespace Gfx_SsrPass {

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_Texture        myOutput{};
    Rhi_Texture        mySceneColor{};
    Rhi_Texture        myHistoryWrite{};
};

struct TracePush {
    alignas( 16 ) glm::mat4 view{ 1.0f };
    alignas( 16 ) glm::mat4 proj{ 1.0f };
    alignas( 16 ) glm::mat4 prevViewProj{ 1.0f };
    alignas( 16 ) glm::vec4 params{};
    alignas( 16 ) glm::uvec4 trace{};
    alignas( 8 ) glm::vec2 screenSize{};
    alignas( 4 ) float historyValid     = 0.0f;
    alignas( 4 ) float depthRejectSigma = 0.0f;
};
static_assert( sizeof( TracePush ) == 240, "TracePush must match SsrTrace.comp" );

struct TraceInput {
    uint32_t         myWidth       = 0;
    uint32_t         myHeight      = 0;
    bool             myDebugLabels = false;
    TracePush        myPush{};
    Rhi_ImageLayout* myOutputLayout = nullptr;
};

struct HistoryInput {
    uint32_t         myWidth         = 0;
    uint32_t         myHeight        = 0;
    Rhi_ImageLayout* mySceneLayout   = nullptr;
    Rhi_ImageLayout* myHistoryLayout = nullptr;
};

void RecordTrace( Rhi_CommandList& aCmd, const GpuResources& aGpu, TraceInput& aInput );
void RecordHistoryUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, HistoryInput& aInput );

}  // namespace Gfx_SsrPass

#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>

namespace Gfx_SsrPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

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

struct PassState {
    Rhi_Pipeline                                        myPipeline{};
    Rhi_PipelineLayout                                  myLayout{};
    Rhi_DescriptorSetLayout                             mySetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Sampler                                         myGBufferSampler{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > mySets{};
    Rhi_Texture                                         mySsrOutput{};
    Rhi_Texture                                         myHistory0{};
    Rhi_Texture                                         myHistory1{};
    Rhi_ImageLayout                                     mySsrLayout = Rhi_ImageLayout::Undefined;
    std::array< Rhi_ImageLayout, 2 >                    myHistoryLayouts{ Rhi_ImageLayout::Undefined, Rhi_ImageLayout::Undefined };
    uint32_t                                            myWidth         = 0;
    uint32_t                                            myHeight        = 0;
    bool                                                myPipelineReady = false;
    bool                                                myImagesReady   = false;
    bool                                                mySetsAllocated = false;
};

struct PipelineInitDesc {
    const void* mySpirvCode      = nullptr;
    size_t      mySpirvBytes     = 0;
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

struct ImageInitDesc {
    uint32_t myWidth  = 0;
    uint32_t myHeight = 0;
};

struct DescriptorUpdateDesc {
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
    uint32_t    myFrameIndex     = 0xffffffffu;  // 0xffffffff = all frames; else only that slot
    Rhi_Texture myGBufferDepth{};
    Rhi_Texture myGBufferNormal{};
    Rhi_Texture myGBufferWorldPos{};
    Rhi_Texture myGBufferAlbedo{};
    Rhi_Texture myHistoryRead{};
    Rhi_Texture myHiZ{};
    Rhi_Sampler myHiZSampler{};
};

[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );

[[nodiscard]] bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState );

void DestroyImages( Rhi_Device& aDevice, PassState& aState );
void DestroyPipeline( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

void RecordTrace( Rhi_CommandList& aCmd, const GpuResources& aGpu, TraceInput& aInput );
void RecordHistoryUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, HistoryInput& aInput );

}  // namespace Gfx_SsrPass

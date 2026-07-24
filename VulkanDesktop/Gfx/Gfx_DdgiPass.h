#pragma once

#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

// Module: Gfx_DdgiPass — probe irradiance/visibility atlas Init + Record via Rhi (no vulkan.h).
namespace Gfx_DdgiPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

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

struct PassState {
    Rhi_Pipeline                                        myPipeline{};
    Rhi_PipelineLayout                                  myLayout{};
    Rhi_DescriptorSetLayout                             mySetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Sampler                                         myGBufferSampler{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > mySets{};
    Rhi_Texture                                         myIrradianceAtlas{};
    Rhi_Texture                                         myVisibilityAtlas{};
    uint32_t                                            myAtlasWidth    = 0;
    uint32_t                                            myAtlasHeight   = 0;
    bool                                                myPipelineReady = false;
    bool                                                myImagesReady   = false;
    bool                                                mySetsAllocated = false;
};

struct PipelineInitDesc {
    const void* mySpirvCode      = nullptr;
    size_t      mySpirvBytes     = 0;
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

// Atlas layout: width = probeCountX * probeCountY, height = probeCountZ (see Vk_DeferredLightingPass CreateDdgiAtlas history).
struct ImageInitDesc {
    uint32_t myProbeCountX = 0;
    uint32_t myProbeCountY = 0;
    uint32_t myProbeCountZ = 0;
};

struct DescriptorUpdateDesc {
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
    Rhi_Texture myWorldPos{};
    Rhi_Texture myNormalRoughness{};
};

[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );

[[nodiscard]] bool CreateOrRecreateImages( Rhi_Device& aDevice, const ImageInitDesc& aDesc, PassState& aState );

void UpdateDescriptors( Rhi_Device& aDevice, const DescriptorUpdateDesc& aDesc, PassState& aState );

void DestroyImages( Rhi_Device& aDevice, PassState& aState );
void DestroyPipeline( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

void RecordProbeUpdate( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_DdgiPass

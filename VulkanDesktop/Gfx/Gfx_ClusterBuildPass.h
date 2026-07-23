#pragma once

#include "../RenderContract/Gpu_ClusterPush.h"
#include "../Rhi/Rhi_CommandList.h"
#include "../Rhi/Rhi_Device.h"
#include "../Rhi/Rhi_Enums.h"
#include "../Rhi/Rhi_Handles.h"

#include <array>
#include <cstdint>

// Module: Gfx_ClusterBuildPass — cluster light-list compute Init + Record via Rhi (no vulkan.h).

namespace Gfx_ClusterBuildPass {

constexpr uint32_t kMaxFramesInFlight = 3u;

struct GpuResources {
    Rhi_Pipeline       myPipeline{};
    Rhi_PipelineLayout myLayout{};
    Rhi_DescriptorSet  mySet{};
    Rhi_Buffer         myLightsBuffer{};
    Rhi_Buffer         myClusterListBuffer{};
};

struct RecordInput {
    uint32_t clusterCount = 0;
    uint32_t lightCount   = 0;
    bool     debugLabels  = false;
};

struct PassState {
    Rhi_Pipeline                                        myPipeline{};
    Rhi_PipelineLayout                                  myLayout{};
    Rhi_DescriptorSetLayout                             mySetLayout{};
    Rhi_DescriptorPool                                  myPool{};
    Rhi_Buffer                                          myLightsBuffer{};
    void*                                               myLightsMapped = nullptr;
    std::array< Rhi_Buffer, kMaxFramesInFlight >        myClusterListBuffers{};
    std::array< Rhi_DescriptorSet, kMaxFramesInFlight > mySets{};
    uint32_t                                            myClusterCount  = 0;
    uint32_t                                            myWidth         = 0;
    uint32_t                                            myHeight        = 0;
    bool                                                mySetsAllocated = false;
    bool                                                myPipelineReady = false;
    bool                                                myInitialized   = false;
};

struct PipelineInitDesc {
    const void* mySpirvCode      = nullptr;
    size_t      mySpirvBytes     = 0;
    uint32_t    myFramesInFlight = kMaxFramesInFlight;
};

struct ListsInitDesc {
    uint32_t myWidth          = 0;
    uint32_t myHeight         = 0;
    uint32_t myFramesInFlight = kMaxFramesInFlight;
};

[[nodiscard]] bool CreatePipeline( Rhi_Device& aDevice, const PipelineInitDesc& aDesc, PassState& aState );

[[nodiscard]] bool CreateOrRecreateLists( Rhi_Device& aDevice, const ListsInitDesc& aDesc, PassState& aState );

void DestroyLists( Rhi_Device& aDevice, PassState& aState );
void DestroyPipeline( Rhi_Device& aDevice, PassState& aState );
void Destroy( Rhi_Device& aDevice, PassState& aState );

[[nodiscard]] GpuResources MakeGpuResources( const PassState& aState, uint32_t aFrameIndex );

void Record( Rhi_CommandList& aCmd, const GpuResources& aGpu, const RecordInput& aInput );

}  // namespace Gfx_ClusterBuildPass

#pragma once

#include <array>

#include <glm/mat4x4.hpp>

#include "Vk_FrameLimits.h"
#include "Vk_Types.h"

struct VkCommandBuffer_T;
using VkCommandBuffer = VkCommandBuffer_T*;
class Vk_Renderer;

struct Vk_SsrState {
    VkPipeline            myComputePipeline     = VK_NULL_HANDLE;
    VkPipelineLayout      myPipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout myDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      myDescriptorPool      = VK_NULL_HANDLE;
    VkSampler             myGBufferSampler      = VK_NULL_HANDLE;

    Gfx_Texture mySsrOutput{};
    // Previous-frame lit HDR (scene color after hybrid resolve). Ping-pong for temporal SSR radiance.
    Gfx_Texture myLitHdrHistory[ 2 ]{};

    std::array< VkDescriptorSet, MAX_FRAMES_IN_FLIGHT > myDescriptorSets{};

    glm::mat4 myPrevViewProj{ 1.0f };    // viewProj at last RecordHistoryUpdate (reprojection matrix)
    glm::vec3 myPrevCameraEye{ 0.0f };   // camera jump invalidates history
    uint32_t  myHistoryWriteIndex = 0u;  // index of buffer last written; SSR reads this buffer next frame
    bool      myHistoryValid      = false;

    bool myInitialized = false;
};

namespace Vk_SsrPass {

void Init( Vk_Renderer& aCore );
void Destroy( Vk_Renderer& aCore );
void RecreateForExtent( Vk_Renderer& aCore );

void RecordCompute( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer, uint32_t aFrameIndex );
void RecordHistoryUpdate( Vk_Renderer& aCore, VkCommandBuffer aCommandBuffer );

VkImageView GetOutputImageView( const Vk_Renderer& aCore );

}  // namespace Vk_SsrPass

#pragma once
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

// Per in-flight frame (MAX_FRAMES_IN_FLIGHT): sync objects, command buffer, camera UBO, descriptor set.
struct Vk_FrameData {
    VkSemaphore myPresentSemaphore;  // signaled when swapchain image is ready
    VkSemaphore myRenderSemaphore;   // signaled when GPU finishes this frame's cmd buffer
    VkFence     myRenderFence;

    Vk_DeletionQueue myFrameDeletionQueue;

    VkCommandBuffer myCommandBuffer;

    Vk_AllocatedBuffer myCameraBuffer;   // sizeof(GpuCameraData), one per frame
    VkDescriptorSet    myGlobalDescriptor;  // set 0: camera + env + texture

    // Reserved for per-object UBOs / secondary sets (not wired in the live draw path yet).
    Vk_AllocatedBuffer myObjectBuffer;
    VkDescriptorSet    myObjectDescriptor;
};
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
    VkDescriptorSet    myGlobalDescriptor;  // set 0 (Frame): camera + env + texture - see Vk_DescriptorPolicy.h

    // TODO(descriptor-strategy): Set 2 instance slab + UNIFORM_BUFFER_DYNAMIC; verify per SprintPlan S1 Set-2 task.
    Vk_AllocatedBuffer myObjectBuffer;
    VkDescriptorSet    myObjectDescriptor;
};
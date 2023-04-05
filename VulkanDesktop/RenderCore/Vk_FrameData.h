#pragma once
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct FrameData {
    VkSemaphore myPresentSemaphore;
    VkSemaphore myRenderSemaphore;
    VkFence     myRenderFence;

    DeletionQueue myFrameDeletionQueue;

    // VkCommandPool   myCommandPool;
    VkCommandBuffer myCommandBuffer;

    AllocatedBuffer myCameraBuffer;
    VkDescriptorSet myGlobalDescriptor;

    AllocatedBuffer myObjectBuffer;
    VkDescriptorSet myObjectDescriptor;
};
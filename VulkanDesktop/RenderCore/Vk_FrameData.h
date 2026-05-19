#pragma once
#include "Vk_DataStruct.h"
#include "Vk_Types.h"

struct Vk_FrameData {
    VkSemaphore myPresentSemaphore;
    VkSemaphore myRenderSemaphore;
    VkFence     myRenderFence;

    Vk_DeletionQueue myFrameDeletionQueue;

    // VkCommandPool   myCommandPool;
    VkCommandBuffer myCommandBuffer;

    Vk_AllocatedBuffer myCameraBuffer;
    VkDescriptorSet myGlobalDescriptor;

    Vk_AllocatedBuffer myObjectBuffer;
    VkDescriptorSet myObjectDescriptor;
};
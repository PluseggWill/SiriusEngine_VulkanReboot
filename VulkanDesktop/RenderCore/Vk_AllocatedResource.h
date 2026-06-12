#pragma once

#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vk_AllocatedImage {
    VkImage       myImage;
    VmaAllocation myAllocation;
};

struct Vk_AllocatedBuffer {
    VkBuffer      myBuffer;
    VmaAllocation myAllocation;
};

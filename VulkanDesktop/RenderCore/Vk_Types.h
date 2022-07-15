#pragma once
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct AllocatedImage {
    VkImage       myImage;
    VmaAllocation myAllocation;
};

struct AllocatedBuffer {
    VkBuffer      myBuffer;
    VmaAllocation myAllocation;
};
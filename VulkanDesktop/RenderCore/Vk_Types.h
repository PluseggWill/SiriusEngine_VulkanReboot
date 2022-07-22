#pragma once
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct AllocatedImage {
public:
    VkImage       myImage;
    VmaAllocation myAllocation;
};

struct AllocatedBuffer {
public:
    VkBuffer      myBuffer;
    VmaAllocation myAllocation;
};

struct Texture {
public:
    AllocatedImage myAllocImage;
    VkImageView    myImageView;

    VkImage GetImage() {
        return myAllocImage.myImage;
    }

    VmaAllocation GetAlloc() {
        return myAllocImage.myAllocation;
    }
};

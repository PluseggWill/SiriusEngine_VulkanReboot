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

class Texture {
private:
    AllocatedImage myAllocImage;
    VkImageView    myImageView;

public:
    VkImage& Image() {
        return myAllocImage.myImage;
    }

    VmaAllocation& Allocation() {
        return myAllocImage.myAllocation;
    }

    VkImageView& ImageView() {
        return myImageView;
    }

    AllocatedImage& AllocImage() {
        return myAllocImage;
    }
};

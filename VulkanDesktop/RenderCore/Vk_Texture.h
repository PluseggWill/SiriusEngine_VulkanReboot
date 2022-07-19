#pragma once
#include "Vk_Types.h"

class Texture {
public:
    AllocatedImage myImage;
    VkImageView    myImageView;
};

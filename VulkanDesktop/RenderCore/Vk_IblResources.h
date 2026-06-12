#pragma once

#include "Vk_Types.h"

#include <string>

class Vk_Core;

struct Vk_IblResourcesState {
    bool myInitialized = false;

    Gfx_Texture myIrradiance{};
    Gfx_Texture myPrefilter{};
    Gfx_Texture mySky{};
    Gfx_Texture myBrdfLut{};

    float myPrefilterMaxMipLevel = 0.0f;

    VkSampler myCubemapSampler = VK_NULL_HANDLE;
    VkSampler myBrdfLutSampler = VK_NULL_HANDLE;
};

namespace Vk_IblResources {

constexpr char kDefaultEnvironmentLogicalPath[] = "Data/Environments/default";

void Init( Vk_Core& aCore, const std::string& aEnvironmentLogicalPath );
void Destroy( Vk_Core& aCore );

}  // namespace Vk_IblResources

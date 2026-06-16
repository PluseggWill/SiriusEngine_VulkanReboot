#pragma once

#include "Vk_Types.h"

#include <string>

class Vk_Renderer;

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

void Init( Vk_Renderer& aCore, const std::string& aEnvironmentLogicalPath );
void Destroy( Vk_Renderer& aCore );

}  // namespace Vk_IblResources

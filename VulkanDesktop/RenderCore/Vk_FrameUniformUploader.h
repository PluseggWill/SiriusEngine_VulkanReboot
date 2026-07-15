#pragma once

#include <cstdint>

#include <glm/glm.hpp>

class Vk_Renderer;
struct Vk_ActiveRenderView;

// Per-frame UBO upload (phase-2 #7): camera in PrepareFrameCpu, env in DrawFrameGpu.

class Vk_FrameUniformUploader {

public:
    static void UpdateForView( const Vk_Renderer& aCore, uint32_t aCurrentFrame, uint32_t aViewIndex, const Vk_ActiveRenderView& aView );

    static void UpdateEnvironment( const Vk_Renderer& aCore, uint32_t aCurrentFrame );

    static void UpdateLightingGlobals( const Vk_Renderer& aCore, uint32_t aCurrentFrame, const glm::mat4& aLightViewProj );

    static void UpdateLightingGlobalsFromScene( const Vk_Renderer& aCore, uint32_t aCurrentFrame );
};

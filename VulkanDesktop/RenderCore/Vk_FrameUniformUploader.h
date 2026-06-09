#pragma once

#include <cstdint>

class Vk_Core;
class Vk_Camera;

// Per-frame UBO upload (phase-2 #7): camera in PrepareFrameCpu, env in DrawFrameGpu.
class Vk_FrameUniformUploader {
public:
    static void UpdateForView( const Vk_Core& aCore, uint32_t aCurrentFrame, uint32_t aViewIndex, const Vk_Camera& aCamera );
    static void UpdateEnvironment( const Vk_Core& aCore, uint32_t aCurrentFrame );
};

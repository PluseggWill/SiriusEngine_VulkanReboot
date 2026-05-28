#pragma once

#include <cstdint>

class Vk_Core;

// Vk_FrameUniformUploader: owns per-frame UBO upload orchestration slice (phase-2 #7).
class Vk_FrameUniformUploader {
public:
    static void Update( const Vk_Core& aCore, uint32_t aCurrentFrame );
};

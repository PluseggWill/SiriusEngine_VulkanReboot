#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace Vk_TemporalResetFlag {

constexpr uint32_t None      = 0u;
constexpr uint32_t Resize    = 1u << 0;
constexpr uint32_t SceneSwap = 1u << 1;
constexpr uint32_t CameraCut = 1u << 2;
constexpr uint32_t Manual    = 1u << 3;

}  // namespace Vk_TemporalResetFlag

// Shared temporal history contract for TAA and history-based passes (SSR/AO in S9.4).
struct Vk_TemporalState {
    uint32_t  myHaltonIndex = 0u;
    glm::vec2 myJitterNdc{ 0.0f };
    glm::vec2 myJitterPixel{ 0.0f };
    // Default off: jitter alone looks like camera shake. TAA path force-enables jitter in PrepareViews.
    bool      myJitterEnabled           = false;
    bool      myHistoryValid            = false;
    uint32_t  myPendingResetReasons     = 0u;
    uint32_t  myLastAppliedResetReasons = 0u;
    glm::mat4 myPrevViewProj{ 1.0f };
    glm::mat4 myCurrViewProj{ 1.0f };
    bool      myPrevViewProjValid = false;
    glm::vec3 myPrevCameraEye{ 0.0f };
    bool      myPrevCameraEyeValid = false;
};

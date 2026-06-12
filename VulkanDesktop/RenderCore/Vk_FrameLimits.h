#pragma once

// Shared frame-in-flight limit (sync objects, per-frame UBO slices, FG compute descriptor rings).
// INVARIANT: MAX_FRAMES_IN_FLIGHT <= swapchain imageCount (Vk_SwapchainHost; preferred image count = 3).
constexpr int MAX_FRAMES_IN_FLIGHT = 3;

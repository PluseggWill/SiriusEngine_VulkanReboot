#pragma once

// Per-frame GPU queue outcome after acquire/record (not init-time swapchain creation).
// Application maps RequestShutdown to glfwSetWindowShouldClose; SkipFrame drops the frame without throwing.
enum class Vk_FrameResult {
    Ok,
    SkipFrame,
    RequestShutdown,
};

#pragma once

#include "Vk_ImGuiLayer.h"
#include "Vk_RenderDoc.h"

#include <chrono>
#include <cstdint>

struct GLFWwindow;

// GLFW window, ImGui frame, RenderDoc, frame timing (platform slice).
struct Vk_PlatformContext {
    uint32_t    myWidth  = 0;
    uint32_t    myHeight = 0;
    GLFWwindow* myWindow = nullptr;

    Vk_ImGuiLayer myImGuiLayer;
    Vk_RenderDoc  myRenderDoc;

    std::chrono::high_resolution_clock::time_point myLastFrameTime;
    bool                                           myHasLastFrameTime = false;

    std::chrono::high_resolution_clock::time_point myFrameInputSampleTime;
    bool                                           myHasFrameInputSampleTime = false;
};

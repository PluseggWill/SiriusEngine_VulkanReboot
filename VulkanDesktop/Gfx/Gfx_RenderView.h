#pragma once

#include <cstdint>

#include <glm/glm.hpp>

inline constexpr uint32_t kGfxMaxRenderViews = 2;

enum class Gfx_RenderViewCameraSource : uint8_t {
    Fly = 0,
    SceneCamera = 1,
};

struct Gfx_RenderView {
    Gfx_RenderViewCameraSource myCameraSource = Gfx_RenderViewCameraSource::Fly;
    // Used when myCameraSource == SceneCamera.
    uint32_t mySceneCameraIndex = 0;
    // Normalized viewport (x, y, width, height) in [0, 1].
    glm::vec4 myViewport{ 0.0f, 0.0f, 1.0f, 1.0f };
    uint32_t  myLayerMask = 0xFFFFFFFFu;
};

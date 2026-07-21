#pragma once

#include <cstdint>

#include <glm/glm.hpp>

namespace UtilTemporalPanel {

// Snapshot of temporal debug fields for ImGui (synced App ↔ RenderCore each frame).
struct State {
    bool      myJitterEnabled = false;
    uint32_t  myHaltonIndex   = 0u;
    glm::vec2 myJitterNdc{ 0.0f };
    glm::vec2 myJitterPixel{ 0.0f };
    bool      myHistoryValid            = false;
    uint32_t  myLastAppliedResetReasons = 0u;
};

struct Actions {
    bool myRequestManualReset = false;
};

void BuildContents( State& aState, bool aTaaEnabled, Actions& aOutActions );

}  // namespace UtilTemporalPanel

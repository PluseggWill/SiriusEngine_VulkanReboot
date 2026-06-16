#pragma once

// Runtime post-process tuning (ImGui → Renderer session).
struct GpuPostSettings {
    float myExposure       = 1.2f;
    bool  myTonemapEnabled = true;
    bool  myBloomEnabled   = false;
    float myBloomThreshold = 1.0f;
    float myBloomIntensity = 0.35f;
    int   myTonemapMode    = 1;  // 0 = Reinhard, 1 = ACES
};

#pragma once

// CPU-side post-process tuning (HybridDeferred; ImGui via Util_LightingPanel).
struct Gfx_PostSettings {
    float myExposure       = 1.0f;
    bool  myTonemapEnabled = true;
    bool  myBloomEnabled   = false;
    float myBloomThreshold = 1.0f;
    float myBloomIntensity = 0.35f;
    // 0 = Reinhard, 1 = ACES (Narkowicz fit).
    int myTonemapMode = 1;
};

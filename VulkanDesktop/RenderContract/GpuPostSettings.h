#pragma once

// Runtime post-process tuning (ImGui → Renderer session).
struct GpuPostSettings {
    float myExposure         = 1.2f;
    bool  myTaaEnabled       = false;
    float myTaaBlend         = 0.875f;  // history weight; lower = sharper / more responsive
    float myTaaVarianceGamma = 1.25f;   // YCoCg variance clip gamma
    float myTaaSharpen       = 0.35f;   // post-resolve unsharp amount
    bool  myTonemapEnabled   = true;
    bool  myBloomEnabled     = false;
    float myBloomThreshold   = 1.0f;
    float myBloomIntensity   = 0.35f;
    int   myTonemapMode      = 1;  // 0 = Reinhard, 1 = ACES
};

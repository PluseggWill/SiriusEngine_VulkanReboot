#pragma once

#include <array>
#include <cstdint>

constexpr int FRAME_HISTORY_COUNT = 120;

struct FrameStats {
    float    myFps            = 0.f;
    float    myAvgFps         = 0.f;
    float    myFps1PercentLow = 0.f;
    float    myFrameMs        = 0.f;
    uint32_t myDrawCalls      = 0;
    uint32_t myPipelineBinds  = 0;

    std::array< float, FRAME_HISTORY_COUNT > myFrameHistory{};
    int myFrameHistoryIndex  = 0;
    int myHistorySampleCount = 0;

    void ResetPerFrameCounters();
    void PushFrameTime( float aFrameMs );

private:
    void UpdateAggregates();
};

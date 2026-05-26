#pragma once

#include <array>
#include <cstdint>

constexpr int FRAME_HISTORY_COUNT = 120;

struct Util_FrameStats {
    float    myFps            = 0.f;
    float    myAvgFps         = 0.f;
    float    myFps1PercentLow = 0.f;
    float    myFrameMs        = 0.f;
    uint32_t myDrawCalls         = 0;
    uint32_t myPipelineBinds     = 0;
    uint32_t myMaterialSetBinds  = 0;

    // Post-cull draw stream (set each frame before record; not cleared by ResetPerFrameCounters).
    uint32_t myActiveEntities           = 0;
    uint32_t myVisibleOpaqueDraws       = 0;
    uint32_t myVisibleTransparentDraws  = 0;
    uint32_t myOpaqueBatchRuns          = 0;
    uint32_t myTransparentBatchRuns     = 0;

    std::array< float, FRAME_HISTORY_COUNT > myFrameHistory{};
    int myFrameHistoryIndex  = 0;
    int myHistorySampleCount = 0;

    void ResetPerFrameCounters();
    void PushFrameTime( float aFrameMs );
    void SetDrawStreamMetrics( uint32_t aActiveEntities, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws, uint32_t aOpaqueBatchRuns,
                               uint32_t aTransparentBatchRuns );

private:
    void UpdateAggregates();
};

#pragma once

#include <array>
#include <cstdint>

constexpr int FRAME_HISTORY_COUNT = 120;

struct Util_FrameStats {
    float    myFps              = 0.f;
    float    myAvgFps           = 0.f;
    float    myFps1PercentLow   = 0.f;
    float    myFrameMs          = 0.f;
    uint32_t myDrawCalls        = 0;
    uint32_t myPipelineBinds    = 0;
    uint32_t myMaterialSetBinds = 0;

    // Post-cull draw stream (set each frame before record; not cleared by ResetPerFrameCounters).
    uint32_t myActiveEntities          = 0;
    uint32_t myVisibleOpaqueDraws      = 0;
    uint32_t myVisibleTransparentDraws = 0;
    uint32_t myOpaqueBatchRuns         = 0;
    uint32_t myTransparentBatchRuns    = 0;

    // Input latency (last completed frame; see RecordInputLatency).
    float myInputToPresentMs      = 0.f;  // Sample() end → vkQueuePresentKHR return (CPU+GPU queue).
    float myGpuFenceWaitMs        = 0.f;  // vkWaitForFences at frame start (GPU back-pressure).
    float myEstimatedDisplayLagMs = 0.f;  // Heuristic: ~1 frame when vsync FIFO is on.
    float myEstimatedTotalLagMs   = 0.f;  // inputToPresent + display lag estimate.
    float mySmoothedTotalLagMs    = 0.f;  // EMA for overlay plot.

    std::array< float, FRAME_HISTORY_COUNT > myFrameHistory{};
    std::array< float, FRAME_HISTORY_COUNT > myInputLagHistory{};
    int                                      myFrameHistoryIndex  = 0;
    int                                      myHistorySampleCount = 0;

    void ResetPerFrameCounters();
    void PushFrameTime( float aFrameMs );
    void SetDrawStreamMetrics( uint32_t aActiveEntities, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws, uint32_t aOpaqueBatchRuns,
                               uint32_t aTransparentBatchRuns );
    void RecordInputLatency( float aInputToPresentMs, float aGpuFenceWaitMs, bool aVsyncFifo, float aFrameMs );

private:
    void UpdateAggregates();
};

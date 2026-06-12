#include "Util_FrameStats.h"

#include <algorithm>
#include <cmath>

void Util_FrameStats::ResetPerFrameCounters() {
    myDrawCalls        = 0;
    myPipelineBinds    = 0;
    myMaterialSetBinds = 0;
}

void Util_FrameStats::SetDrawStreamMetrics( uint32_t aActiveEntities, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws, uint32_t aOpaqueBatchRuns,
                                            uint32_t aTransparentBatchRuns ) {
    myActiveEntities          = aActiveEntities;
    myVisibleOpaqueDraws      = aVisibleOpaqueDraws;
    myVisibleTransparentDraws = aVisibleTransparentDraws;
    myOpaqueBatchRuns         = aOpaqueBatchRuns;
    myTransparentBatchRuns    = aTransparentBatchRuns;
}

void Util_FrameStats::RecordInputLatency( float aInputToPresentMs, float aGpuFenceWaitMs, bool aVsyncFifo, float aFrameMs ) {
    myInputToPresentMs      = aInputToPresentMs;
    myGpuFenceWaitMs        = aGpuFenceWaitMs;
    myVsyncFifo             = aVsyncFifo;
    myEstimatedDisplayLagMs = aVsyncFifo ? aFrameMs : 0.f;
    myEstimatedTotalLagMs   = aInputToPresentMs + myEstimatedDisplayLagMs;

    if ( mySmoothedTotalLagMs <= 0.f ) {
        mySmoothedTotalLagMs = myEstimatedTotalLagMs;
    }
    else {
        constexpr float kAlpha = 0.15f;
        mySmoothedTotalLagMs   = mySmoothedTotalLagMs * ( 1.f - kAlpha ) + myEstimatedTotalLagMs * kAlpha;
    }

    if ( myHistorySampleCount > 0 ) {
        const int lagIndex                                     = ( myFrameHistoryIndex - 1 + FRAME_HISTORY_COUNT ) % FRAME_HISTORY_COUNT;
        myInputLagHistory[ static_cast< size_t >( lagIndex ) ] = myEstimatedTotalLagMs;
    }
}

void Util_FrameStats::SetPendingFrameBreakdown( float aWorkMs, float aPresentWaitMs, float aGpuFenceWaitMs, bool aVsyncFifo ) {
    myPendingFrameWorkMs    = aWorkMs;
    myPendingPresentWaitMs  = aPresentWaitMs;
    myPendingGpuFenceWaitMs = aGpuFenceWaitMs;
    myVsyncFifo             = aVsyncFifo;
}

void Util_FrameStats::PushFrameTime( float aFrameMs ) {
    myFrameMs = aFrameMs;
    if ( aFrameMs > 0.f )
        myFps = 1000.f / aFrameMs;

    myFrameWorkMs   = myPendingFrameWorkMs;
    myPresentWaitMs = myPendingPresentWaitMs;

    // Commit wall-clock + breakdown for the frame that just finished (pending set in prior DrawFrameGpu).
    const int index                                         = myFrameHistoryIndex;
    myFrameHistory[ static_cast< size_t >( index ) ]        = aFrameMs;
    myFrameWorkHistory[ static_cast< size_t >( index ) ]    = myPendingFrameWorkMs;
    myPresentWaitHistory[ static_cast< size_t >( index ) ]  = myPendingPresentWaitMs;
    myGpuFenceWaitHistory[ static_cast< size_t >( index ) ] = myPendingGpuFenceWaitMs;
    myFrameHistoryIndex                                     = ( myFrameHistoryIndex + 1 ) % FRAME_HISTORY_COUNT;
    myHistorySampleCount                                    = std::min( myHistorySampleCount + 1, FRAME_HISTORY_COUNT );

    UpdateAggregates();
}

void Util_FrameStats::UpdateAggregates() {
    const int sampleCount = std::min( myHistorySampleCount, FRAME_HISTORY_COUNT );
    if ( sampleCount <= 0 ) {
        myAvgFps         = 0.f;
        myFps1PercentLow = 0.f;
        return;
    }

    std::array< float, FRAME_HISTORY_COUNT > frameTimesMs{};
    float                                    sumMs = 0.f;
    // Ring buffer: myFrameHistoryIndex is next write slot; walk backward for chronological aggregates.
    for ( int i = 0; i < sampleCount; i++ ) {
        const int ringIndex                        = ( myFrameHistoryIndex - sampleCount + i + FRAME_HISTORY_COUNT ) % FRAME_HISTORY_COUNT;
        frameTimesMs[ static_cast< size_t >( i ) ] = myFrameHistory[ static_cast< size_t >( ringIndex ) ];
        sumMs += frameTimesMs[ static_cast< size_t >( i ) ];
    }

    const float avgMs = sumMs / static_cast< float >( sampleCount );
    myAvgFps          = avgMs > 0.f ? 1000.f / avgMs : 0.f;

    // 1% Low: mean frametime of the slowest 1% of samples, converted to FPS (CapFrameX-style).
    const int lowCount = std::max( 1, static_cast< int >( std::ceil( sampleCount * 0.01f ) ) );
    std::sort( frameTimesMs.begin(), frameTimesMs.begin() + sampleCount, std::greater< float >() );

    float slowSumMs = 0.f;
    for ( int i = 0; i < lowCount; i++ )
        slowSumMs += frameTimesMs[ static_cast< size_t >( i ) ];

    const float slowAvgMs = slowSumMs / static_cast< float >( lowCount );
    myFps1PercentLow      = slowAvgMs > 0.f ? 1000.f / slowAvgMs : 0.f;
}

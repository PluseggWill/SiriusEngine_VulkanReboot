#include "Util_FrameStats.h"

#include <algorithm>
#include <cmath>

void Util_FrameStats::ResetPerFrameCounters() {
    myDrawCalls        = 0;
    myPipelineBinds    = 0;
    myMaterialSetBinds = 0;
}

void Util_FrameStats::SetDrawStreamMetrics( uint32_t aActiveEntities, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws,
                                            uint32_t aOpaqueBatchRuns, uint32_t aTransparentBatchRuns ) {
    myActiveEntities          = aActiveEntities;
    myVisibleOpaqueDraws      = aVisibleOpaqueDraws;
    myVisibleTransparentDraws = aVisibleTransparentDraws;
    myOpaqueBatchRuns         = aOpaqueBatchRuns;
    myTransparentBatchRuns    = aTransparentBatchRuns;
}

void Util_FrameStats::PushFrameTime( float aFrameMs ) {
    myFrameMs = aFrameMs;
    if ( aFrameMs > 0.f )
        myFps = 1000.f / aFrameMs;

    myFrameHistory[ static_cast< size_t >( myFrameHistoryIndex ) ] = aFrameMs;
    myFrameHistoryIndex = ( myFrameHistoryIndex + 1 ) % FRAME_HISTORY_COUNT;
    myHistorySampleCount = std::min( myHistorySampleCount + 1, FRAME_HISTORY_COUNT );

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
    for ( int i = 0; i < sampleCount; i++ ) {
        frameTimesMs[ static_cast< size_t >( i ) ] = myFrameHistory[ static_cast< size_t >( i ) ];
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

#include "FrameStats.h"

#include <algorithm>
#include <cmath>

void FrameStats::ResetPerFrameCounters() {
    myDrawCalls     = 0;
    myPipelineBinds = 0;
}

void FrameStats::PushFrameTime( float aFrameMs ) {
    myFrameMs = aFrameMs;
    if ( aFrameMs > 0.f )
        myFps = 1000.f / aFrameMs;

    myFrameHistory[ static_cast< size_t >( myFrameHistoryIndex ) ] = aFrameMs;
    myFrameHistoryIndex = ( myFrameHistoryIndex + 1 ) % FRAME_HISTORY_COUNT;
}

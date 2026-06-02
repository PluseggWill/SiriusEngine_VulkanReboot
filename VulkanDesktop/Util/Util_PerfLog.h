#pragma once

#include <cstdint>

namespace UtilPerfLog {

// Append one JSONL row per completed DrawFrame when UtilEngineConfig::GetPerfLogPath() is set.
// schemaVersion 1: frameIndex, frameMs, drawCalls, visibleDraws (summed across active views), activeViews, materialPath.
void AppendFrame( uint64_t aFrameIndex, float aFrameMs, uint32_t aDrawCalls, uint32_t aVisibleDraws, uint32_t aActiveViews, const char* aMaterialPath );

}  // namespace UtilPerfLog

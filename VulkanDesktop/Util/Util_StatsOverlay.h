#pragma once

struct Util_FrameStats;

namespace UtilStatsOverlay {
// Parent window/tab must already be open (no Begin/End here).
void BuildContents( const Util_FrameStats& aStats );
}  // namespace UtilStatsOverlay

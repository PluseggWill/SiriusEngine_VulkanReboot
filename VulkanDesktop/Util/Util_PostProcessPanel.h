#pragma once

#include "../Gfx/Gfx_PostSettings.h"

namespace UtilPostProcessPanel {

// Side effects applied by App after the panel (no RenderCore in Util).
struct Actions {
    bool myClearTaaHistoryReady       = false;
    bool myRequestTemporalManualReset = false;
};

// Parent window/tab must already be open (no Begin/End here).
void BuildContents( bool aHybridDeferred, Gfx_PostSettings& aPostSettings, Actions& aOutActions );

}  // namespace UtilPostProcessPanel

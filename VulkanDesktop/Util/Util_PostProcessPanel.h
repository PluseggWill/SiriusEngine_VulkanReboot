#pragma once

#include "Util_EngineConfig.h"

struct Gfx_PostSettings;

namespace UtilPostProcessPanel {

// Parent window/tab must already be open (no Begin/End here).
void BuildContents( const Util_EngineConfig& aConfig, Gfx_PostSettings& aPostSettings );

}  // namespace UtilPostProcessPanel

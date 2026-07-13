#pragma once

#include "../Gfx/Gfx_PostSettings.h"
#include "Util_EngineConfig.h"

class Vk_Renderer;

namespace UtilPostProcessPanel {

// Parent window/tab must already be open (no Begin/End here).
void BuildContents( const Util_EngineConfig& aConfig, Gfx_PostSettings& aPostSettings, Vk_Renderer& aCore );

}  // namespace UtilPostProcessPanel

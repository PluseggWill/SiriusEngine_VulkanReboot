#pragma once

#include "../Util/Util_EngineConfig.h"
#include "Gfx_SceneDesc.h"

#include <string>

// Load and parse scene JSON. aLogicalPath is repo-relative under aConfig.GetAssetRoot().
// Throws std::runtime_error prefixed with [SCENE] on schema/version/IO errors.
Gfx_SceneDesc Gfx_LoadSceneDesc( const Util_EngineConfig& aConfig, const std::string& aLogicalPath );

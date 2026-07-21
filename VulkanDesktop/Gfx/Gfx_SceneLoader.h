#pragma once

#include "Gfx_SceneDesc.h"

#include <filesystem>
#include <string>

// Load and parse scene JSON. aLogicalPath is repo-relative under aAssetRoot.
// Throws std::runtime_error prefixed with [SCENE] on schema/version/IO errors.
Gfx_SceneDesc Gfx_LoadSceneDesc( const std::filesystem::path& aAssetRoot, const std::string& aLogicalPath );

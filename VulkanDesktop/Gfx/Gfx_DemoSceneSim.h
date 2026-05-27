#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "Gfx_SceneSoA.h"

// Demo-only: optional Z spin on scene base transforms before extract. No Vulkan.

void Gfx_TickDemoSceneTransforms( Gfx_SceneSoA& aScene, const std::vector< glm::mat4 >& aBaseTransforms );

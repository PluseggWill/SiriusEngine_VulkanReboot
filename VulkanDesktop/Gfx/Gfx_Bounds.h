#pragma once

#include <array>

#include <glm/glm.hpp>

class Gfx_SceneSoA;

struct Gfx_Bounds {
    glm::vec3 myMin{ 0.0f };
    glm::vec3 myMax{ 0.0f };
};

glm::vec3                  Gfx_BoundsCenter( const Gfx_Bounds& aBounds );
Gfx_Bounds                 Gfx_TransformBounds( const Gfx_Bounds& aLocalBounds, const glm::mat4& aWorldTransform );
std::array< glm::vec3, 8 > Gfx_BoundsCorners( const Gfx_Bounds& aBounds );
Gfx_Bounds                 Gfx_ComputeActiveOpaqueSceneBounds( const Gfx_SceneSoA& aScene );

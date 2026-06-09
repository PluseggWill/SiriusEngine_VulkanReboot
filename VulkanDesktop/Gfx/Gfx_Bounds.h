#pragma once

#include <glm/glm.hpp>

struct Gfx_Bounds {
    glm::vec3 myMin{ 0.0f };
    glm::vec3 myMax{ 0.0f };
};

glm::vec3  Gfx_BoundsCenter( const Gfx_Bounds& aBounds );
Gfx_Bounds Gfx_TransformBounds( const Gfx_Bounds& aLocalBounds, const glm::mat4& aWorldTransform );

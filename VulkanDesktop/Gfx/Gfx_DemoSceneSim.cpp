#include "Gfx_DemoSceneSim.h"

#include "../Util/Util_EngineConfig.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

namespace {

glm::mat4 ComputeDemoSpinMatrix( const glm::mat4& aWorldTransform, float aTimeSeconds, bool aDemoRotate ) {
    const glm::mat4 spin = glm::rotate( glm::mat4( 1.0f ), aDemoRotate ? aTimeSeconds * glm::radians( 90.0f ) : 0.0f, glm::vec3( 0.0f, 0.0f, 1.0f ) );
    return aWorldTransform * spin;
}

}  // namespace

void Gfx_TickDemoSceneTransforms( const Util_EngineConfig& aConfig, Gfx_SceneTransformState& aTransforms ) {
    static auto startTime   = std::chrono::high_resolution_clock::now();
    const auto  currentTime = std::chrono::high_resolution_clock::now();
    const float timeSeconds = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
    const bool  demoRotate  = aConfig.GetFeatures().myDemoRotate;

    if ( aTransforms.myResolvedWorldTransforms.size() < aTransforms.mySourceWorldTransforms.size() ) {
        aTransforms.myResolvedWorldTransforms.resize( aTransforms.mySourceWorldTransforms.size(), glm::mat4( 1.0f ) );
    }

    for ( size_t slot = 0; slot < aTransforms.mySourceWorldTransforms.size(); ++slot ) {
        const glm::mat4& base                         = aTransforms.mySourceWorldTransforms[ slot ];
        aTransforms.myResolvedWorldTransforms[ slot ] = ComputeDemoSpinMatrix( base, timeSeconds, demoRotate );
    }
}

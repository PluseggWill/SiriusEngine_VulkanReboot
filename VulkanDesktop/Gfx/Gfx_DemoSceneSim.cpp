#include "Gfx_DemoSceneSim.h"

#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

namespace {

using Clock = std::chrono::high_resolution_clock;

Clock::time_point gDemoSimStartTime = Clock::now();

glm::mat4 ComputeDemoSpinMatrix( const glm::mat4& aWorldTransform, float aTimeSeconds ) {
    const glm::mat4 spin = glm::rotate( glm::mat4( 1.0f ), aTimeSeconds * glm::radians( 90.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );
    return aWorldTransform * spin;
}

}  // namespace

void Gfx_ResetDemoSceneSimTime() {
    gDemoSimStartTime = Clock::now();
}

void Gfx_TickDemoSceneTransforms( bool aDemoRotateEnabled, Gfx_SceneTransformState& aTransforms ) {
    if ( aTransforms.myResolvedWorldTransforms.size() < aTransforms.mySourceWorldTransforms.size() ) {
        aTransforms.myResolvedWorldTransforms.resize( aTransforms.mySourceWorldTransforms.size(), glm::mat4( 1.0f ) );
    }

    if ( !aDemoRotateEnabled ) {
        for ( size_t slot = 0; slot < aTransforms.mySourceWorldTransforms.size(); ++slot ) {
            aTransforms.myResolvedWorldTransforms[ slot ] = aTransforms.mySourceWorldTransforms[ slot ];
        }
        return;
    }

    const float timeSeconds = std::chrono::duration< float, std::chrono::seconds::period >( Clock::now() - gDemoSimStartTime ).count();
    for ( size_t slot = 0; slot < aTransforms.mySourceWorldTransforms.size(); ++slot ) {
        aTransforms.myResolvedWorldTransforms[ slot ] = ComputeDemoSpinMatrix( aTransforms.mySourceWorldTransforms[ slot ], timeSeconds );
    }
}

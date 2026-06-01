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

void Gfx_TickDemoSceneTransforms( Gfx_SceneSoA& aScene, const std::vector< glm::mat4 >& aBaseTransforms ) {
    static auto startTime   = std::chrono::high_resolution_clock::now();
    const auto  currentTime = std::chrono::high_resolution_clock::now();
    const float timeSeconds = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
    const bool  demoRotate  = UtilEngineConfig::GetFeatures().myDemoRotate;

    for ( const uint32_t slot : aScene.GetActiveSlots() ) {
        if ( slot >= aBaseTransforms.size() ) {
            continue;
        }
        const glm::mat4& base  = aBaseTransforms[ slot ];
        const glm::mat4  world = ComputeDemoSpinMatrix( base, timeSeconds, demoRotate );
        aScene.SetTransform( slot, world );
    }
}

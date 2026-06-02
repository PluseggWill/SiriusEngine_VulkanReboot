#include "Gfx_SceneTransform.h"

void Gfx_SceneTransformState::Clear() {
    mySourceWorldTransforms.clear();
    myResolvedWorldTransforms.clear();
}

void Gfx_ResolveFlatWorldTransforms( const Gfx_SceneTransformState& aTransforms, Gfx_SceneSoA& aScene ) {
    for ( const uint32_t slot : aScene.GetActiveSlots() ) {
        if ( slot < aTransforms.myResolvedWorldTransforms.size() ) {
            aScene.SetWorldTransform( slot, aTransforms.myResolvedWorldTransforms[ slot ] );
            continue;
        }
        // Fallback keeps flat scenes robust during partial state migrations/reloads.
        if ( slot < aTransforms.mySourceWorldTransforms.size() ) {
            aScene.SetWorldTransform( slot, aTransforms.mySourceWorldTransforms[ slot ] );
        }
    }
}

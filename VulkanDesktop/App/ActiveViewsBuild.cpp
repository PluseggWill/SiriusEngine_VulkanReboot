#include "ActiveViewsBuild.h"

#include "../Gfx/Gfx_RenderCamera.h"
#include "../Gfx/Gfx_SceneDesc.h"
#include "DebugUIState.h"
#include "WorldState.h"
#include <algorithm>

namespace {

glm::vec4 ClampNormalizedViewport( const glm::vec4& aViewport ) {
    const float x = std::clamp( aViewport.x, 0.0f, 1.0f );
    const float y = std::clamp( aViewport.y, 0.0f, 1.0f );
    const float w = std::clamp( aViewport.z, 0.0f, 1.0f - x );
    const float h = std::clamp( aViewport.w, 0.0f, 1.0f - y );
    return { x, y, w, h };
}

bool IsPiPViewport( const glm::vec4& aViewportNorm ) {
    return aViewportNorm.z < 0.95f || aViewportNorm.w < 0.95f;
}

uint32_t ResolvePiPCameraIndex( const WorldState& aWorld, uint32_t aRequestedIndex ) {
    const auto& cameras = aWorld.myLoadedScene.myCameras;
    if ( cameras.empty() ) {
        return 0;
    }

    const uint32_t clamped = std::min( aRequestedIndex, static_cast< uint32_t >( cameras.size() - 1 ) );
    if ( IsPiPViewport( ClampNormalizedViewport( cameras[ clamped ].myViewport ) ) ) {
        return clamped;
    }

    for ( uint32_t i = 0; i < static_cast< uint32_t >( cameras.size() ); ++i ) {
        if ( IsPiPViewport( ClampNormalizedViewport( cameras[ i ].myViewport ) ) ) {
            return i;
        }
    }

    return clamped;
}

void CopyCameraState( Gfx_ActiveRenderView& aView, const Gfx_RenderCamera& aCamera ) {
    aView.myCameraView = aCamera.myView;
    aView.myCameraProj = aCamera.myProj;
    aView.myCameraEye  = aCamera.myEye;
}

}  // namespace

std::array< Gfx_ActiveRenderView, kGfxMaxRenderViews > BuildActiveRenderViews( uint32_t& aOutViewCount, const WorldState& aWorld, const DebugUIState& aDebugUI,
                                                                               const Gfx_RenderCamera& aFlyCamera ) {
    std::array< Gfx_ActiveRenderView, kGfxMaxRenderViews > views{};
    aOutViewCount = 1;

    views[ 0 ].myView.myCameraSource = Gfx_RenderViewCameraSource::Fly;
    views[ 0 ].myView.myViewport     = glm::vec4( 0.0f, 0.0f, 1.0f, 1.0f );
    views[ 0 ].myView.myLayerMask    = 0xFFFFFFFFu;
    CopyCameraState( views[ 0 ], aFlyCamera );

    // PiP: secondary view from scene JSON camera (viewport/layer); stays in App so RenderCore prep stays JSON-free.
    if ( aDebugUI.myMultiView.myEnablePiP && !aWorld.myLoadedScene.myCameras.empty() ) {
        aOutViewCount                            = 2;
        const uint32_t              cameraIndex  = ResolvePiPCameraIndex( aWorld, aDebugUI.myMultiView.mySecondaryCameraIndex );
        const Gfx_SceneCameraEntry& sceneCamera  = aWorld.myLoadedScene.myCameras[ cameraIndex ];
        const glm::vec4             viewportNorm = ClampNormalizedViewport( sceneCamera.myViewport );

        views[ 1 ].myView.myCameraSource     = Gfx_RenderViewCameraSource::SceneCamera;
        views[ 1 ].myView.mySceneCameraIndex = cameraIndex;
        views[ 1 ].myView.myViewport         = viewportNorm;
        views[ 1 ].myView.myLayerMask        = sceneCamera.myLayerMask;

        const float      viewportAspect = viewportNorm.w > 0.0f ? ( viewportNorm.z / viewportNorm.w ) * aFlyCamera.myAspect : aFlyCamera.myAspect;
        Gfx_RenderCamera secondaryCamera;
        secondaryCamera.SetLens( sceneCamera.myFovYDeg, aFlyCamera.myNear, aFlyCamera.myFar, viewportAspect );
        secondaryCamera.LookAt( sceneCamera.myEye, sceneCamera.myCenter, sceneCamera.myUp );
        CopyCameraState( views[ 1 ], secondaryCamera );
    }

    return views;
}

#include "ActiveViewsBuild.h"

#include "../Gfx/Gfx_SceneDesc.h"
#include "../RenderCore/Vk_Camera.h"
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

VkViewport ToViewport( const VkExtent2D& aExtent, const glm::vec4& aViewportNorm ) {
    const float x      = aViewportNorm.x * static_cast< float >( aExtent.width );
    const float y      = aViewportNorm.y * static_cast< float >( aExtent.height );
    const float width  = std::max( 1.0f, aViewportNorm.z * static_cast< float >( aExtent.width ) );
    const float height = std::max( 1.0f, aViewportNorm.w * static_cast< float >( aExtent.height ) );
    return VkViewport{ x, y, width, height, 0.0f, 1.0f };
}

VkRect2D ToScissor( const VkExtent2D& aExtent, const VkViewport& aViewport ) {
    VkRect2D scissor{};
    scissor.offset.x = std::max( 0, static_cast< int32_t >( aViewport.x ) );
    scissor.offset.y = std::max( 0, static_cast< int32_t >( aViewport.y ) );

    const uint32_t offsetX             = static_cast< uint32_t >( scissor.offset.x );
    const uint32_t offsetY             = static_cast< uint32_t >( scissor.offset.y );
    const uint32_t maxWidthFromOffset  = offsetX < aExtent.width ? ( aExtent.width - offsetX ) : 1u;
    const uint32_t maxHeightFromOffset = offsetY < aExtent.height ? ( aExtent.height - offsetY ) : 1u;
    scissor.extent.width               = std::max( 1u, std::min( maxWidthFromOffset, static_cast< uint32_t >( aViewport.width ) ) );
    scissor.extent.height              = std::max( 1u, std::min( maxHeightFromOffset, static_cast< uint32_t >( aViewport.height ) ) );
    return scissor;
}

}  // namespace

std::array< Vk_ActiveRenderView, kGfxMaxRenderViews > BuildActiveRenderViews( uint32_t& aOutViewCount, const WorldState& aWorld, const DebugUIState& aDebugUI,
                                                                              const Vk_Camera& aFlyCamera, VkExtent2D aSwapChainExtent ) {
    std::array< Vk_ActiveRenderView, kGfxMaxRenderViews > views{};
    aOutViewCount = 1;

    views[ 0 ].myView.myCameraSource = Gfx_RenderViewCameraSource::Fly;
    views[ 0 ].myView.myViewport     = glm::vec4( 0.0f, 0.0f, 1.0f, 1.0f );
    views[ 0 ].myView.myLayerMask    = 0xFFFFFFFFu;
    views[ 0 ].myCamera              = aFlyCamera;
    views[ 0 ].myViewport            = ToViewport( aSwapChainExtent, views[ 0 ].myView.myViewport );
    views[ 0 ].myScissor             = ToScissor( aSwapChainExtent, views[ 0 ].myViewport );

    // PiP: secondary view from scene JSON camera (viewport/layer); stays in App so Vk_Core prep stays JSON-free.
    if ( aDebugUI.myMultiView.myEnablePiP && !aWorld.myLoadedScene.myCameras.empty() ) {
        aOutViewCount              = 2;
        const uint32_t cameraIndex = ResolvePiPCameraIndex( aWorld, aDebugUI.myMultiView.mySecondaryCameraIndex );
        const Gfx_SceneCameraEntry& sceneCamera  = aWorld.myLoadedScene.myCameras[ cameraIndex ];
        const glm::vec4             viewportNorm = ClampNormalizedViewport( sceneCamera.myViewport );

        views[ 1 ].myView.myCameraSource     = Gfx_RenderViewCameraSource::SceneCamera;
        views[ 1 ].myView.mySceneCameraIndex = cameraIndex;
        views[ 1 ].myView.myViewport         = viewportNorm;
        views[ 1 ].myView.myLayerMask        = sceneCamera.myLayerMask;
        views[ 1 ].myViewport                = ToViewport( aSwapChainExtent, viewportNorm );
        views[ 1 ].myScissor                 = ToScissor( aSwapChainExtent, views[ 1 ].myViewport );

        const float aspect = static_cast< float >( views[ 1 ].myScissor.extent.width ) / static_cast< float >( views[ 1 ].myScissor.extent.height );
        views[ 1 ].myCamera.SetLens( sceneCamera.myFovYDeg, aFlyCamera.myNear, aFlyCamera.myFar, aspect );
        views[ 1 ].myCamera.LookAt( sceneCamera.myEye, sceneCamera.myCenter, sceneCamera.myUp );
    }

    return views;
}

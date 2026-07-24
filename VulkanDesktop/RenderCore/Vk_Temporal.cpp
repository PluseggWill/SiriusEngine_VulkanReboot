#include "Vk_Temporal.h"

#include "../Gfx/Gfx_TemporalJitter.h"
#include "Vk_FrameLimits.h"
#include "Vk_Renderer.h"

#include <algorithm>

namespace Vk_Temporal {

namespace {

    void InvalidatePassHistoryReady( Vk_Renderer& aCore ) {
        // Pass-owned history textures stay allocated; clear "buffer ready" so consumers skip blend this frame.
        aCore.myAoState.myTemporalHistoryReady           = false;
        aCore.mySsrState.myHistoryReady                  = false;
        aCore.myPostProcessState.myGfx.myTaaHistoryReady = false;
    }

}  // namespace

void RequestReset( Vk_Renderer& aCore, const uint32_t aReasonFlags ) {
    aCore.myTemporalState.myPendingResetReasons |= aReasonFlags;
}

void NotifyResize( Vk_Renderer& aCore ) {
    RequestReset( aCore, Vk_TemporalResetFlag::Resize );
}

void NotifySceneSwap( Vk_Renderer& aCore ) {
    RequestReset( aCore, Vk_TemporalResetFlag::SceneSwap );
    aCore.myTemporalState.myPrevCameraEyeValid = false;
    aCore.myTemporalState.myPrevViewProjValid  = false;
}

void PrepareViews( Vk_Renderer& aCore, std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews, const uint32_t aViewCount ) {
    Vk_TemporalState& state  = aCore.myTemporalState;
    const VkExtent2D  extent = aCore.GetSwapChainExtent();

    uint32_t resetReasons       = state.myPendingResetReasons;
    state.myPendingResetReasons = 0u;

    const uint32_t activeViewCount = std::min( aViewCount, kGfxMaxRenderViews );
    if ( activeViewCount > 0 ) {
        const glm::vec3 eye = aViews[ 0 ].myCameraEye;
        if ( state.myPrevCameraEyeValid && glm::length( eye - state.myPrevCameraEye ) > kCameraCutEyeDistance ) {
            resetReasons |= Vk_TemporalResetFlag::CameraCut;
        }
        state.myPrevCameraEye      = eye;
        state.myPrevCameraEyeValid = true;
    }

    if ( resetReasons != Vk_TemporalResetFlag::None ) {
        state.myHistoryValid            = false;
        state.myLastAppliedResetReasons = resetReasons;
        if ( ( resetReasons & Vk_TemporalResetFlag::Resize ) != 0u ) {
            state.myHaltonIndex = 0u;
        }
        InvalidatePassHistoryReady( aCore );
    }
    else {
        state.myLastAppliedResetReasons = Vk_TemporalResetFlag::None;
    }

    // TAA requires subpixel jitter; without TAA only apply when the Temporal debug checkbox is on.
    const bool applyJitter = ( aCore.myPostSettings.myTaaEnabled || state.myJitterEnabled ) && extent.width > 0 && extent.height > 0 && activeViewCount > 0;
    if ( applyJitter ) {
        state.myJitterNdc = Gfx_TemporalJitter::SampleNdc( state.myHaltonIndex, extent.width, extent.height );
        state.myJitterPixel =
            glm::vec2( state.myJitterNdc.x * static_cast< float >( extent.width ) * 0.5f, state.myJitterNdc.y * static_cast< float >( extent.height ) * 0.5f );
        aViews[ 0 ].myCameraProj = Gfx_TemporalJitter::ApplyToProjection( aViews[ 0 ].myCameraProj, state.myJitterNdc );
        state.myHaltonIndex      = ( state.myHaltonIndex + 1u ) % Gfx_TemporalJitter::kSequenceLength;
    }
    else {
        state.myJitterNdc   = glm::vec2( 0.0f );
        state.myJitterPixel = glm::vec2( 0.0f );
    }

    if ( activeViewCount > 0 ) {
        state.myCurrViewProj = aViews[ 0 ].myCameraProj * aViews[ 0 ].myCameraView;
        if ( !state.myPrevViewProjValid ) {
            state.myPrevViewProj      = state.myCurrViewProj;
            state.myPrevViewProjValid = true;
        }
        if ( resetReasons != Vk_TemporalResetFlag::None ) {
            state.myPrevViewProj = state.myCurrViewProj;
        }
    }
}

const char* ResetReasonLabel( const uint32_t aReasonFlags ) {
    if ( aReasonFlags == Vk_TemporalResetFlag::None ) {
        return "none";
    }
    if ( aReasonFlags == Vk_TemporalResetFlag::Resize ) {
        return "resize";
    }
    if ( aReasonFlags == Vk_TemporalResetFlag::SceneSwap ) {
        return "scene swap";
    }
    if ( aReasonFlags == Vk_TemporalResetFlag::CameraCut ) {
        return "camera cut";
    }
    if ( aReasonFlags == Vk_TemporalResetFlag::Manual ) {
        return "manual";
    }
    return "multiple";
}

}  // namespace Vk_Temporal

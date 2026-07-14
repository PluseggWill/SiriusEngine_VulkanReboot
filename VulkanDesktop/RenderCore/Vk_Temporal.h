#pragma once

#include "Vk_ActiveRenderView.h"
#include "Vk_FrameLimits.h"
#include "Vk_TemporalState.h"

#include <array>

class Vk_Renderer;

namespace Vk_Temporal {

constexpr float kCameraCutEyeDistance = 2.5f;  // eye jump treated as cut → shared history reset

void RequestReset( Vk_Renderer& aCore, uint32_t aReasonFlags );
void NotifyResize( Vk_Renderer& aCore );
void NotifySceneSwap( Vk_Renderer& aCore );

// Apply Halton jitter to primary view and evaluate shared history reset contract. Call after BuildActiveRenderViews.
void PrepareViews( Vk_Renderer& aCore, std::array< Vk_ActiveRenderView, kGfxMaxRenderViews >& aViews, uint32_t aViewCount );

const char* ResetReasonLabel( uint32_t aReasonFlags );

}  // namespace Vk_Temporal

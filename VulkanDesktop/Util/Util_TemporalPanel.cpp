#include "Util_TemporalPanel.h"

#include "../Gfx/Gfx_TemporalJitter.h"
#include "../RenderCore/Vk_Renderer.h"
#include "../RenderCore/Vk_Temporal.h"
#include "../RenderCore/Vk_TemporalState.h"

#include <imgui.h>

namespace UtilTemporalPanel {

void BuildContents( Vk_Renderer& aCore ) {
    Vk_TemporalState& state = aCore.myTemporalState;

    ImGui::Checkbox( "Halton jitter enabled", &state.myJitterEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Debug: force jitter without TAA. While TAA is enabled, jitter is always applied." );
    }
    if ( aCore.GetPostSettings().myTaaEnabled ) {
        ImGui::TextDisabled( "TAA on: jitter forced this frame." );
    }

    const uint32_t displayIndex = state.myHaltonIndex == 0u ? ( Gfx_TemporalJitter::kSequenceLength - 1u ) : ( state.myHaltonIndex - 1u );
    ImGui::Text( "Halton index (last applied): %u / %u", displayIndex, Gfx_TemporalJitter::kSequenceLength - 1u );
    ImGui::Text( "Jitter NDC: (%.6f, %.6f)", state.myJitterNdc.x, state.myJitterNdc.y );
    ImGui::Text( "Jitter px:  (%.3f, %.3f)", state.myJitterPixel.x, state.myJitterPixel.y );
    ImGui::Text( "History valid: %s", state.myHistoryValid ? "yes" : "no" );
    ImGui::Text( "Last reset: %s", Vk_Temporal::ResetReasonLabel( state.myLastAppliedResetReasons ) );

    if ( ImGui::Button( "Invalidate temporal history" ) ) {
        Vk_Temporal::RequestReset( aCore, Vk_TemporalResetFlag::Manual );
    }
}

}  // namespace UtilTemporalPanel

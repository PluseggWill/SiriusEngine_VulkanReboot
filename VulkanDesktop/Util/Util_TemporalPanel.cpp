#include "Util_TemporalPanel.h"

#include "../Gfx/Gfx_TemporalJitter.h"

#include <imgui.h>

namespace UtilTemporalPanel {

namespace {

    const char* ResetReasonLabel( uint32_t aReasonFlags ) {
        // Keep labels in sync with Vk_TemporalResetFlag bit meanings.
        if ( aReasonFlags == 0u ) {
            return "none";
        }
        if ( aReasonFlags == ( 1u << 0 ) ) {
            return "resize";
        }
        if ( aReasonFlags == ( 1u << 1 ) ) {
            return "scene swap";
        }
        if ( aReasonFlags == ( 1u << 2 ) ) {
            return "camera cut";
        }
        if ( aReasonFlags == ( 1u << 3 ) ) {
            return "manual";
        }
        return "multiple";
    }

}  // namespace

void BuildContents( State& aState, bool aTaaEnabled, Actions& aOutActions ) {
    aOutActions = {};

    ImGui::Checkbox( "Halton jitter enabled", &aState.myJitterEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Debug: force jitter without TAA. While TAA is enabled, jitter is always applied." );
    }
    if ( aTaaEnabled ) {
        ImGui::TextDisabled( "TAA on: jitter forced this frame." );
    }

    const uint32_t displayIndex = aState.myHaltonIndex == 0u ? ( Gfx_TemporalJitter::kSequenceLength - 1u ) : ( aState.myHaltonIndex - 1u );
    ImGui::Text( "Halton index (last applied): %u / %u", displayIndex, Gfx_TemporalJitter::kSequenceLength - 1u );
    ImGui::Text( "Jitter NDC: (%.6f, %.6f)", aState.myJitterNdc.x, aState.myJitterNdc.y );
    ImGui::Text( "Jitter px:  (%.3f, %.3f)", aState.myJitterPixel.x, aState.myJitterPixel.y );
    ImGui::Text( "History valid: %s", aState.myHistoryValid ? "yes" : "no" );
    ImGui::Text( "Last reset: %s", ResetReasonLabel( aState.myLastAppliedResetReasons ) );
    ImGui::TextDisabled( "Reset also clears TAA / AO temporal / SSR history-ready flags." );

    if ( ImGui::Button( "Invalidate temporal history" ) ) {
        aOutActions.myRequestManualReset = true;
    }
}

}  // namespace UtilTemporalPanel

#include "Util_PostProcessPanel.h"

#include "../Gfx/Gfx_PostSettings.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "Util_EngineConfig.h"

#include <imgui.h>

namespace UtilPostProcessPanel {

void BuildContents( const Util_EngineConfig& aConfig, Gfx_PostSettings& aPostSettings ) {
    const bool hybridDeferred = Gfx_RenderPreset::IsHybridDeferred( aConfig.GetRenderPresetName() );
    if ( !hybridDeferred ) {
        ImGui::TextDisabled( "Post-process (tonemap / bloom) requires render preset HybridDeferred." );
        return;
    }

    ImGui::TextDisabled( "HDR scene color -> optional bloom -> tonemap to swapchain." );

    ImGui::Separator();
    ImGui::Text( "Tonemap" );
    ImGui::Checkbox( "Tonemap enabled", &aPostSettings.myTonemapEnabled );
    ImGui::SliderFloat( "Exposure", &aPostSettings.myExposure, 0.1f, 4.f );
    const char* tonemapModes[] = { "Reinhard", "ACES" };
    int         tonemapMode    = aPostSettings.myTonemapMode;
    if ( ImGui::Combo( "Tonemap mode", &tonemapMode, tonemapModes, 2 ) ) {
        aPostSettings.myTonemapMode = tonemapMode;
    }

    ImGui::Separator();
    ImGui::Text( "Bloom" );
    ImGui::Checkbox( "Bloom enabled", &aPostSettings.myBloomEnabled );
    ImGui::SliderFloat( "Bloom threshold", &aPostSettings.myBloomThreshold, 0.5f, 4.f );
    ImGui::SliderFloat( "Bloom intensity", &aPostSettings.myBloomIntensity, 0.f, 2.f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Adds blurred HDR highlights after threshold; modulated in tonemap pass." );
    }
}

}  // namespace UtilPostProcessPanel

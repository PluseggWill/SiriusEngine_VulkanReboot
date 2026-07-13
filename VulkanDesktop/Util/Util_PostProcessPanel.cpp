#include "Util_PostProcessPanel.h"

#include "../Gfx/Gfx_PostSettings.h"
#include "../Gfx/Gfx_RenderPreset.h"
#include "../RenderCore/Vk_Renderer.h"
#include "../RenderCore/Vk_Temporal.h"
#include "Util_EngineConfig.h"

#include <imgui.h>

namespace UtilPostProcessPanel {

void BuildContents( const Util_EngineConfig& aConfig, Gfx_PostSettings& aPostSettings, Vk_Renderer& aCore ) {
    const bool hybridDeferred = Gfx_RenderPreset::IsHybridDeferred( aConfig.GetRenderPresetName() );
    if ( !hybridDeferred ) {
        ImGui::TextDisabled( "Post-process (tonemap / bloom) requires render preset HybridDeferred." );
        return;
    }

    ImGui::TextDisabled( "HDR scene color -> optional TAA -> optional bloom -> tonemap to swapchain." );

    ImGui::Separator();
    ImGui::Text( "Temporal AA (v0.5)" );
    const bool taaWasEnabled = aPostSettings.myTaaEnabled;
    if ( ImGui::Checkbox( "TAA enabled", &aPostSettings.myTaaEnabled ) ) {
        if ( aPostSettings.myTaaEnabled && !taaWasEnabled ) {
            aCore.myPostProcessState.myTaaHistoryReady = false;
            Vk_Temporal::RequestReset( aCore, Vk_TemporalResetFlag::Manual );
        }
        else if ( !aPostSettings.myTaaEnabled && taaWasEnabled ) {
            aCore.myPostProcessState.myTaaHistoryReady = false;
        }
    }
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Halton jitter + Catmull-Rom history + YCoCg variance clip + sharpen." );
    }
    ImGui::SliderFloat( "TAA history blend", &aPostSettings.myTaaBlend, 0.0f, 0.98f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Base history weight (velocity increases current-frame weight). Lower = sharper." );
    }
    ImGui::SliderFloat( "TAA variance gamma", &aPostSettings.myTaaVarianceGamma, 0.5f, 2.5f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "YCoCg variance clip AABB = mu +/- gamma*sigma. Lower = less ghosting, more flicker." );
    }
    ImGui::SliderFloat( "TAA sharpen", &aPostSettings.myTaaSharpen, 0.0f, 1.5f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Unsharp amount using current-frame high frequencies after resolve." );
    }

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

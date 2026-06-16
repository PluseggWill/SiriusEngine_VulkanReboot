#include "Util_RenderDebugPanel.h"

#include "../RenderCore/Vk_Types.h"
#include "Util_EngineConfig.h"

#include <algorithm>
#include <imgui.h>

namespace UtilRenderDebugPanel {

void BuildContents( const Util_EngineConfig& aConfig, State& aState, GpuEnvironmentData& anEnvironment, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws ) {
    const char* debugViewLabels[] = { "Lit", "Depth", "World normal", "Shadow map", "AO", "Hi-Z" };
    int         debugViewIndex    = static_cast< int >( aState.myDebugViewMode );
    if ( ImGui::Combo( "Debug view", &debugViewIndex, debugViewLabels, IM_ARRAYSIZE( debugViewLabels ) ) ) {
        debugViewIndex         = std::clamp( debugViewIndex, 0, 5 );
        aState.myDebugViewMode = static_cast< Gfx_DebugViewMode >( debugViewIndex );
    }

    if ( aState.myDebugViewMode == Gfx_DebugViewMode_HiZ ) {
        int hiZMip = static_cast< int >( aState.myHiZDebugMip );
        if ( ImGui::SliderInt( "Hi-Z mip", &hiZMip, 0, 7 ) ) {
            aState.myHiZDebugMip = static_cast< uint32_t >( std::clamp( hiZMip, 0, 7 ) );
        }
    }

    ImGui::Checkbox( "Skip opaque pass", &aState.mySkipOpaquePass );
    ImGui::Checkbox( "Skip transparent pass", &aState.mySkipTransparentPass );
    ImGui::Checkbox( "CPU LOD", &aState.myLodEnabled );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "Session toggle; default from engine.json features.lodEnabled / CLI." );
    }

    ImGui::Separator();
    const std::string& preset = aConfig.GetRenderPresetName();
    ImGui::Text( "Render preset: %s", preset.empty() ? "(none)" : preset.c_str() );
    ImGui::Text( "Shader permutation: %s", aConfig.GetShaderPermutationName().c_str() );
    ImGui::TextDisabled( "Preset switch requires restart (CLI / engine.json)." );

    ImGui::Separator();
    ImGui::Text( "Visible draws: opaque %u  transparent %u", aVisibleOpaqueDraws, aVisibleTransparentDraws );

    // CONTRACT: fogDistances.w encodes Gfx_DebugViewMode for TriangleFrag_Lit*.frag (xyz = lighting tunables).
    anEnvironment.myFogDistance.w = Gfx_DebugViewModeToShaderPacked( aState.myDebugViewMode );
}

}  // namespace UtilRenderDebugPanel

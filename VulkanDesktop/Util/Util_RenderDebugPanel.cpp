#include "Util_RenderDebugPanel.h"

#include "Util_EngineConfig.h"

#include <algorithm>
#include <imgui.h>

namespace UtilRenderDebugPanel {

void Build( const Util_EngineConfig& aConfig, State& aState, GpuEnvironmentData& anEnvironment, uint32_t aVisibleOpaqueDraws, uint32_t aVisibleTransparentDraws ) {
    ImGui::SetNextWindowPos( ImVec2( 10.f, 420.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowBgAlpha( 0.9f );
    ImGui::Begin( "Render Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

    const char* debugViewLabels[] = { "Lit", "Depth", "World normal" };
    int         debugViewIndex    = static_cast< int >( aState.myDebugViewMode );
    if ( ImGui::Combo( "Debug view", &debugViewIndex, debugViewLabels, IM_ARRAYSIZE( debugViewLabels ) ) ) {
        debugViewIndex         = std::clamp( debugViewIndex, 0, 2 );
        aState.myDebugViewMode = static_cast< Gfx_DebugViewMode >( debugViewIndex );
    }

    ImGui::Checkbox( "Skip opaque pass", &aState.mySkipOpaquePass );
    ImGui::Checkbox( "Skip transparent pass", &aState.mySkipTransparentPass );

    ImGui::Separator();
    const std::string& preset = aConfig.GetRenderPresetName();
    ImGui::Text( "Render preset: %s", preset.empty() ? "(none)" : preset.c_str() );
    ImGui::Text( "Shader permutation: %s", aConfig.GetShaderPermutationName().c_str() );
    ImGui::TextDisabled( "Preset switch requires restart (CLI / engine.json)." );

    ImGui::Separator();
    ImGui::Text( "Visible draws: opaque %u  transparent %u", aVisibleOpaqueDraws, aVisibleTransparentDraws );

    ImGui::End();

    // CONTRACT: fogDistances.w encodes Gfx_DebugViewMode for TriangleFrag_Lit*.frag (xyz = lighting tunables).
    anEnvironment.myFogDistance.w = Gfx_DebugViewModeToShaderPacked( aState.myDebugViewMode );
}

}  // namespace UtilRenderDebugPanel

#include "Util_StatsOverlay.h"

#include "../Util/Util_FrameStats.h"

#include <algorithm>
#include <imgui.h>

void UtilStatsOverlay::Build( const Util_FrameStats& aStats ) {
    ImGui::SetNextWindowPos( ImVec2( 10.f, 10.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowBgAlpha( 0.85f );
    ImGui::Begin( "Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

    ImGui::Text( "Hello, Sirius Engine" );
    ImGui::Separator();
    ImGui::Text( "FPS: %.1f", aStats.myFps );
    ImGui::Text( "Avg FPS: %.1f  (last %d frames)", aStats.myAvgFps, std::min( aStats.myHistorySampleCount, FRAME_HISTORY_COUNT ) );
    ImGui::Text( "1%% Low: %.1f", aStats.myFps1PercentLow );
    ImGui::Text( "Frame: %.2f ms", aStats.myFrameMs );
    ImGui::PlotLines( "Frame ms", aStats.myFrameHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 33.f, ImVec2( 0.f, 80.f ) );
    ImGui::Separator();
    ImGui::Text( "Draw calls: %u", aStats.myDrawCalls );
    ImGui::Text( "Pipeline binds: %u", aStats.myPipelineBinds );
    ImGui::Text( "Material set binds: %u", aStats.myMaterialSetBinds );

    ImGui::End();
}

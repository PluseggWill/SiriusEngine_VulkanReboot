#include "StatsOverlay.h"

#include "../Util/FrameStats.h"

#include <imgui.h>

void StatsOverlay_Build( const FrameStats& aStats ) {
    ImGui::SetNextWindowPos( ImVec2( 10.f, 10.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowBgAlpha( 0.85f );
    ImGui::Begin( "Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

    ImGui::Text( "Hello, Sirius Engine" );
    ImGui::Separator();
    ImGui::Text( "FPS: %.1f", aStats.myFps );
    ImGui::Text( "Frame: %.2f ms", aStats.myFrameMs );
    ImGui::PlotLines( "Frame ms", aStats.myFrameHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 33.f, ImVec2( 0.f, 80.f ) );
    ImGui::Separator();
    ImGui::Text( "Draw calls: %u", aStats.myDrawCalls );
    ImGui::Text( "Pipeline binds: %u", aStats.myPipelineBinds );

    ImGui::End();
}

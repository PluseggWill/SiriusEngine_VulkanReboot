#include "Util_StatsOverlay.h"

#include "../Util/Util_FrameStats.h"

#include <algorithm>
#include <imgui.h>

void UtilStatsOverlay::BuildContents( const Util_FrameStats& aStats ) {
    ImGui::Text( "Hello, Sirius Engine" );
    ImGui::Separator();
    ImGui::Text( "FPS: %.1f", aStats.myFps );
    ImGui::Text( "Avg FPS: %.1f  (last %d frames)", aStats.myAvgFps, std::min( aStats.myHistorySampleCount, FRAME_HISTORY_COUNT ) );
    ImGui::Text( "1%% Low: %.1f", aStats.myFps1PercentLow );
    ImGui::Text( "Frame: %.2f ms", aStats.myFrameMs );
    ImGui::PlotLines( "Frame ms", aStats.myFrameHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 33.f, ImVec2( -1.f, 80.f ) );

    if ( aStats.myVsyncFifo && ImGui::CollapsingHeader( "Vsync breakdown", ImGuiTreeNodeFlags_None ) ) {
        ImGui::Text( "Work ms: %.2f   Present wait ms: %.2f", aStats.myFrameWorkMs, aStats.myPresentWaitMs );
        ImGui::TextDisabled( "Work = loop start through submit; Present = vkQueuePresentKHR block." );
        ImGui::TextDisabled( "Frame ms ~ Work + Present (+ small gap before next loop)." );
        ImGui::PlotLines( "Work ms", aStats.myFrameWorkHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 20.f, ImVec2( -1.f, 48.f ) );
        ImGui::PlotLines( "Present wait ms", aStats.myPresentWaitHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 20.f, ImVec2( -1.f, 48.f ) );
    }

    if ( ImGui::CollapsingHeader( "Input latency", ImGuiTreeNodeFlags_None ) ) {
        ImGui::Text( "  sample -> present: %.2f ms", aStats.myInputToPresentMs );
        ImGui::Text( "  GPU fence wait:     %.2f ms", aStats.myGpuFenceWaitMs );
        ImGui::Text( "  display (vsync):    %.2f ms", aStats.myEstimatedDisplayLagMs );
        ImGui::Text( "  total (est.):       %.2f ms", aStats.myEstimatedTotalLagMs );
        ImGui::PlotLines( "Input lag est. ms", aStats.myInputLagHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 50.f, ImVec2( -1.f, 48.f ) );
        ImGui::TextDisabled( "Total = sample->present + ~1 frame if vsync FIFO." );
        ImGui::TextDisabled( "Try --no-vsync to compare present mode." );
    }

    ImGui::PlotLines( "GPU fence wait ms", aStats.myGpuFenceWaitHistory.data(), FRAME_HISTORY_COUNT, aStats.myFrameHistoryIndex, nullptr, 0.f, 20.f, ImVec2( -1.f, 48.f ) );

    if ( ImGui::CollapsingHeader( "Draw stats", ImGuiTreeNodeFlags_None ) ) {
        const uint32_t visibleDraws = aStats.myVisibleOpaqueDraws + aStats.myVisibleTransparentDraws;
        const uint32_t batchRuns    = aStats.myOpaqueBatchRuns + aStats.myTransparentBatchRuns;
        ImGui::Text( "Entities: %u  visible draws: %u", aStats.myActiveEntities, visibleDraws );
        ImGui::Text( "Batch runs: %u (opaque %u + transparent %u)", batchRuns, aStats.myOpaqueBatchRuns, aStats.myTransparentBatchRuns );
        ImGui::Text( "Draw calls: %u", aStats.myDrawCalls );
        ImGui::Text( "Pipeline binds: %u", aStats.myPipelineBinds );
        ImGui::Text( "Material set binds: %u", aStats.myMaterialSetBinds );
        if ( batchRuns > 0 && aStats.myMaterialSetBinds <= batchRuns ) {
            ImGui::TextDisabled( "Set 1 binds <= batch runs (batched path)" );
        }
    }
}

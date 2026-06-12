#include "Gfx_ObjectiveRuntime.h"

#include "../Util/Util_Logger.h"

#include <imgui.h>

namespace {

float HorizontalDistance( const glm::vec3& aFrom, const glm::vec3& aTo ) {
    const glm::vec2 delta = glm::vec2( aFrom.x - aTo.x, aFrom.y - aTo.y );
    return glm::length( delta );
}

void LogOutcomeOnce( Gfx_ObjectiveRuntimeState& aState, const char* aMessage ) {
    if ( aState.myLoggedOutcome ) {
        return;
    }
    UtilLogger::Info( "SLICE", aMessage );
    aState.myLoggedOutcome = true;
}

}  // namespace

void Gfx_ResetObjectiveRuntime( Gfx_ObjectiveRuntimeState& aState ) {
    aState.myOutcome        = Gfx_ObjectiveOutcome::Playing;
    aState.myElapsedSeconds = 0.0f;
    aState.myLoggedOutcome  = false;
}

void Gfx_TickObjectiveRuntime( const Gfx_SceneObjectiveDesc& aObjective, const glm::vec3& aCameraPosition, float aDeltaSeconds, Gfx_ObjectiveRuntimeState& aState ) {
    if ( aObjective.myType == Gfx_SceneObjectiveType::None || aState.myOutcome != Gfx_ObjectiveOutcome::Playing ) {
        return;
    }

    aState.myElapsedSeconds += aDeltaSeconds;

    switch ( aObjective.myType ) {
    case Gfx_SceneObjectiveType::Reach:
        if ( HorizontalDistance( aCameraPosition, aObjective.myTargetPosition ) <= aObjective.myRadius ) {
            aState.myOutcome = Gfx_ObjectiveOutcome::Won;
            LogOutcomeOnce( aState, "Objective won: reached target" );
            return;
        }
        if ( aObjective.myTimeLimitSeconds > 0.0f && aState.myElapsedSeconds >= aObjective.myTimeLimitSeconds ) {
            aState.myOutcome = Gfx_ObjectiveOutcome::Lost;
            LogOutcomeOnce( aState, "Objective lost: time limit" );
        }
        break;
    default:
        break;
    }
}

void Gfx_BuildObjectiveHud( const Gfx_SceneObjectiveDesc& aObjective, const Gfx_ObjectiveRuntimeState& aState, bool aShowHud ) {
    if ( aObjective.myType == Gfx_SceneObjectiveType::None || !aShowHud ) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos( ImVec2( display.x * 0.5f, 10.f ), ImGuiCond_FirstUseEver, ImVec2( 0.5f, 0.f ) );
    ImGui::SetNextWindowBgAlpha( 0.85f );
    ImGui::Begin( "Objective", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav );

    switch ( aObjective.myType ) {
    case Gfx_SceneObjectiveType::Reach: {
        ImGui::TextUnformatted( "Reach the target" );
        const float remaining = aObjective.myTimeLimitSeconds - aState.myElapsedSeconds;
        if ( aState.myOutcome == Gfx_ObjectiveOutcome::Playing ) {
            ImGui::Text( "Time left: %.0fs", remaining > 0.0f ? remaining : 0.0f );
        }
        else if ( aState.myOutcome == Gfx_ObjectiveOutcome::Won ) {
            ImGui::TextColored( ImVec4( 0.3f, 1.f, 0.4f, 1.f ), "Complete!" );
        }
        else {
            ImGui::TextColored( ImVec4( 1.f, 0.35f, 0.3f, 1.f ), "Failed — press R to restart" );
        }
        break;
    }
    default:
        break;
    }

    ImGui::TextDisabled( "R = restart scene" );
    ImGui::End();
}

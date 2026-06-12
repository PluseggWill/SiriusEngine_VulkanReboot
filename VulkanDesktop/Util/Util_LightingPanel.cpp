#include "Util_LightingPanel.h"

#include "../Gfx/Gfx_LightingGlobals.h"
#include "../RenderCore/Vk_Types.h"
#include "Util_Logger.h"

#include <glm/glm.hpp>
#include <imgui.h>

namespace {

void LogLightingSettingsIfChanged( const Gfx_LightingSettings& aSettings ) {
    static bool     sInitialized = false;
    static bool     sShadows     = true;
    static bool     sIbl         = true;
    static float    sIntensity   = 1.0f;
    const bool      changed      = !sInitialized || sShadows != aSettings.myShadowsEnabled || sIbl != aSettings.myIblEnabled
                                || sIntensity != aSettings.myIblIntensity;
    if ( !changed ) {
        return;
    }

    sInitialized = true;
    sShadows     = aSettings.myShadowsEnabled;
    sIbl         = aSettings.myIblEnabled;
    sIntensity   = aSettings.myIblIntensity;
    UtilLogger::Info( "LIGHTING", std::string( "shadows=" ) + ( sShadows ? "1" : "0" ) + " ibl=" + ( sIbl ? "1" : "0" )
                                    + " iblIntensity=" + std::to_string( sIntensity ) );
}

}  // namespace

void UtilLightingPanel::BuildContents( GpuEnvironmentData& anEnvironment, Gfx_LightingSettings& aLightingSettings ) {
    ImGui::ColorEdit3( "Ambient", &anEnvironment.myAmbientColor.x );
    ImGui::ColorEdit3( "Sun color", &anEnvironment.mySunlightColor.x );
    ImGui::DragFloat3( "Sun direction", &anEnvironment.mySunlightDirection.x, 0.02f, -1.f, 1.f );

    if ( ImGui::Button( "Normalize sun direction" ) ) {
        const glm::vec3 dir = glm::vec3( anEnvironment.mySunlightDirection );
        if ( glm::dot( dir, dir ) > 0.0001f ) {
            anEnvironment.mySunlightDirection = glm::vec4( glm::normalize( dir ), 0.f );
        }
    }

    ImGui::Separator();
    ImGui::Checkbox( "Shadows enabled", &aLightingSettings.myShadowsEnabled );
    ImGui::Checkbox( "IBL enabled", &aLightingSettings.myIblEnabled );
    ImGui::SliderFloat( "IBL intensity", &aLightingSettings.myIblIntensity, 0.f, 3.f );
    if ( ImGui::IsItemHovered() ) {
        ImGui::SetTooltip( "When IBL is off, ambient color scales the legacy fallback." );
    }

    ImGui::Separator();
    ImGui::TextDisabled( "Specular / shininess: unused under PBR (material roughness/metallic)" );
    ImGui::BeginDisabled();
    ImGui::SliderFloat( "Specular strength (legacy)", &anEnvironment.myFogDistance.x, 0.f, 2.f );
    ImGui::SliderFloat( "Shininess (legacy)", &anEnvironment.myFogDistance.y, 1.f, 256.f );
    ImGui::EndDisabled();
    ImGui::SliderFloat( "Gfx_Texture blend", &anEnvironment.myFogDistance.z, 0.f, 1.f );

    LogLightingSettingsIfChanged( aLightingSettings );
}

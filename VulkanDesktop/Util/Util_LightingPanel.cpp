#include "Util_LightingPanel.h"

#include "../RenderCore/Vk_Types.h"

#include <glm/glm.hpp>
#include <imgui.h>

void UtilLightingPanel::BuildContents( GpuEnvironmentData& anEnvironment ) {
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
    ImGui::SliderFloat( "Specular strength", &anEnvironment.myFogDistance.x, 0.f, 2.f );
    ImGui::SliderFloat( "Shininess", &anEnvironment.myFogDistance.y, 1.f, 256.f );
    ImGui::SliderFloat( "Gfx_Texture blend", &anEnvironment.myFogDistance.z, 0.f, 1.f );
}

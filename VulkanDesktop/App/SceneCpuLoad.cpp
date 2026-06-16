// Module: SceneCpuLoad - App-owned scene CPU bootstrap and initial fly-camera / env defaults.
#include "SceneCpuLoad.h"

#include "../Gfx/Gfx_LightingMath.h"
#include "../Gfx/Gfx_SceneApply.h"
#include "../RenderCore/Vk_Renderer.h"
#include "WorldState.h"

#include <algorithm>
#include <glm/glm.hpp>

namespace {

bool IsFullViewportCamera( const Gfx_SceneCameraEntry& aCamera ) {
    return aCamera.myViewport.z >= 0.95f && aCamera.myViewport.w >= 0.95f;
}

const Gfx_SceneCameraEntry* FindFlySpawnCamera( const Gfx_SceneDesc& aScene ) {
    for ( const Gfx_SceneCameraEntry& camera : aScene.myCameras ) {
        if ( camera.myId == "spawn" ) {
            return &camera;
        }
    }
    for ( const Gfx_SceneCameraEntry& camera : aScene.myCameras ) {
        if ( IsFullViewportCamera( camera ) ) {
            return &camera;
        }
    }
    return nullptr;
}

}  // namespace

void App_LoadSceneCpuState( WorldState& aWorld ) {
    aWorld.mySceneIdTables = Gfx_BuildSceneIdTables( aWorld.myLoadedScene );
    Gfx_PopulateSceneSoAFromSceneDesc( aWorld.myLoadedScene, aWorld.mySceneIdTables, aWorld.mySceneSoA, aWorld.mySceneTransformState );
    Gfx_BuildLodTableFromSceneDesc( aWorld.myLoadedScene, aWorld.mySceneIdTables, aWorld.myLodTable );
    aWorld.myLodState.Clear();

    const auto treeIt              = aWorld.mySceneIdTables.myLogicalMeshIdByName.find( "tree" );
    aWorld.myLodDebugLogicalMeshId = treeIt != aWorld.mySceneIdTables.myLogicalMeshIdByName.end() ? treeIt->second : UINT32_MAX;
}

void App_InitScenePresentation( Vk_Renderer& aCore, const WorldState& aWorld ) {
    const float aspect = static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.width ) / static_cast< float >( aCore.mySwapchainCtx.mySwapChainExtent.height );

    float     fovDeg    = 45.0f;
    float     nearPlane = 0.1f;
    float     farPlane  = 32.0f;
    glm::vec3 eye{ 10.0f, 0.1f, 16.0f };
    glm::vec3 center{ 9.0f, 0.1f, 16.0f };
    glm::vec3 up{ 0.0f, 0.0f, 1.0f };

    if ( const Gfx_SceneCameraEntry* spawn = FindFlySpawnCamera( aWorld.myLoadedScene ) ) {
        eye                      = spawn->myEye;
        center                   = spawn->myCenter;
        up                       = spawn->myUp;
        fovDeg                   = spawn->myFovYDeg;
        const float lookDistance = glm::length( center - eye );
        farPlane                 = std::max( 32.0f, lookDistance * 10.0f );
    }

    aCore.myCamera.SetLens( fovDeg, nearPlane, farPlane, aspect );
    aCore.myCamera.LookAt( eye, center, up );

    aCore.myEnvironmentData.myAmbientColor      = { 0.24f, 0.26f, 0.30f, 1.0f };
    aCore.myEnvironmentData.myFogColor          = { 0.92f, 0.95f, 1.0f, 1.0f };
    aCore.myEnvironmentData.myFogDistance       = { 0.65f, 48.0f, 1.0f, 0.0f };
    aCore.myEnvironmentData.mySunlightDirection = { glm::vec4( Gfx_LightingMath::Gfx_DefaultSunDirectionTowardLight(), 0.0f ) };
    aCore.myEnvironmentData.mySunlightColor     = { 1.15f, 1.12f, 1.02f, 1.0f };
    aCore.myEnvironmentData.myViewWorldPos      = { aCore.myCamera.myEye, 1.0f };
}

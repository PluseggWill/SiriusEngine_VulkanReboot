#include "Vk_SceneHost.h"

#include "../App/WorldState.h"
#include "Vk_Core.h"

#include "../Gfx/Gfx_SceneApply.h"
#include <glm/glm.hpp>

void Vk_SceneHost::LoadCpuState( WorldState& aWorld, Vk_Core& aCore ) {
    ( void )aCore;  // InitScenePresentation still needs swapchain extent on core (Phase 1).
    aWorld.mySceneIdTables = Gfx_BuildSceneIdTables( aWorld.myLoadedScene );
    Gfx_PopulateSceneSoAFromSceneDesc( aWorld.myLoadedScene, aWorld.mySceneIdTables, aWorld.mySceneSoA, aWorld.mySceneTransformState );
    Gfx_BuildLodTableFromSceneDesc( aWorld.myLoadedScene, aWorld.mySceneIdTables, aWorld.myLodTable );
    aWorld.myLodState.Clear();

    const auto treeIt            = aWorld.mySceneIdTables.myLogicalMeshIdByName.find( "tree" );
    aWorld.myLodDebugLogicalMeshId = treeIt != aWorld.mySceneIdTables.myLogicalMeshIdByName.end() ? treeIt->second : UINT32_MAX;
}

void Vk_SceneHost::InitScenePresentation( Vk_Core& aCore ) {
    // Demo defaults until scene JSON environment blocks exist (lighting epic).
    aCore.myCamera.SetLens( 45.0f, 0.1f, 32.0f, static_cast< float >( aCore.mySwapChainExtent.width ) / static_cast< float >( aCore.mySwapChainExtent.height ) );
    aCore.myCamera.LookAt( glm::vec3( 0.0f, 3.0f, 9.0f ), glm::vec3( 0.0f, 0.5f, -2.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );

    aCore.myEnvironmentData.myAmbientColor      = { 0.15f, 0.15f, 0.18f, 1.0f };
    aCore.myEnvironmentData.myFogColor          = { 1.0f, 1.0f, 1.0f, 1.0f };
    aCore.myEnvironmentData.myFogDistance       = { 0.45f, 32.0f, 1.0f, 0.0f };
    aCore.myEnvironmentData.mySunlightDirection = { glm::normalize( glm::vec3( -0.35f, -0.85f, -0.4f ) ), 0.0f };
    aCore.myEnvironmentData.mySunlightColor     = { 0.9f, 0.88f, 0.82f, 1.0f };
    aCore.myEnvironmentData.myViewWorldPos      = { aCore.myCamera.myEye, 1.0f };
}

#include "Vk_SceneHost.h"

#include "Vk_Core.h"

#include "../Gfx/Gfx_SceneApply.h"

void Vk_SceneHost::LoadCpuState( Vk_Core& aCore ) {
    aCore.mySceneIdTables = Gfx_BuildSceneIdTables( aCore.myLoadedScene );
    Gfx_PopulateSceneSoAFromSceneDesc( aCore.myLoadedScene, aCore.mySceneIdTables, aCore.mySceneSoA, aCore.myDemoBaseTransforms );
    Gfx_BuildLodTableFromSceneDesc( aCore.myLoadedScene, aCore.mySceneIdTables, aCore.myLodTable );
    aCore.myLodState.Clear();

    const auto treeIt = aCore.mySceneIdTables.myLogicalMeshIdByName.find( "tree" );
    aCore.myLodDebugLogicalMeshId = treeIt != aCore.mySceneIdTables.myLogicalMeshIdByName.end() ? treeIt->second : UINT32_MAX;
}

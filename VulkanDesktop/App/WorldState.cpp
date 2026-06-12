#include "WorldState.h"

// CPU half of scene unload - call before Vk_Core::UnloadSceneGpuResources (no Vulkan).
void WorldState::ClearCpuSceneState() {
    mySceneSoA.Clear();
    myLodTable = Gfx_LodTable{};
    myLodState.Clear();
    mySceneTransformState.Clear();
    mySceneIdTables = Gfx_SceneIdTables{};
    myLoadedScene   = Gfx_SceneDesc{};
    myLogicalPath.clear();
    myLodDebugLogicalMeshId = UINT32_MAX;
    myHasLoadedScene        = false;
}

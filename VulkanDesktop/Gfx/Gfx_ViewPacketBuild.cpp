// Module: Gfx_ViewPacketBuild - extract/cull/LOD/sort/batch -> Gfx_FrameRenderPacket (no Vulkan).
#include "Gfx_ViewPacketBuild.h"

void Gfx_BuildViewFramePacket( const Gfx_ViewPacketBuildParams& aParams, Gfx_FrameDrawStreamLogState& aLogs, Gfx_FrameRenderPacket& aOutPacket,
                               size_t& aOutDrawCountBeforeCull ) {
    Gfx_FrameDrawStreamParams streamParams{};
    streamParams.myScene                 = aParams.myScene;
    streamParams.myView.myView           = aParams.myView;
    streamParams.myView.myProj           = aParams.myProj;
    streamParams.myView.myViewLayerMask  = aParams.myViewLayerMask;
    streamParams.myCameraEye             = aParams.myCameraEye;
    streamParams.myCameraView            = aParams.myView;
    streamParams.myLodTable              = aParams.myLodTable;
    streamParams.myLodState              = aParams.myLodState;
    streamParams.myLodEnabled            = aParams.myLodEnabled;
    streamParams.myLodDebugLogicalMeshId = aParams.myLodDebugLogicalMeshId;
    streamParams.myGpuCullEnabled        = aParams.myGpuCullEnabled;

    Gfx_FrameDrawStreamOutput streamOut{};
    Gfx_BuildFrameDrawStream( streamParams, streamOut, aLogs );

    aOutDrawCountBeforeCull = streamOut.myDrawCountBeforeCull;
    Gfx_BuildFrameRenderPacketFromStream( streamOut, aOutPacket );
}

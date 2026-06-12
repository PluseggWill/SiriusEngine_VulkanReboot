#include "Gfx_FrameDrawStream.h"

#include "../Util/Util_Logger.h"
#include "Gfx_Bounds.h"

void Gfx_ResetFrameDrawStreamLogState( Gfx_FrameDrawStreamLogState& aLogs ) {
    aLogs = Gfx_FrameDrawStreamLogState{};
}

void Gfx_BuildFrameDrawStream( const Gfx_FrameDrawStreamParams& aParams, Gfx_FrameDrawStreamOutput& aOut, Gfx_FrameDrawStreamLogState& aLogs ) {
    aOut.myOpaqueBatchRuns.clear();
    aOut.myTransparentBatchRuns.clear();
    aOut.myShadowCasterBatchRuns.clear();

    Gfx_ExtractDrawInstances( *aParams.myScene, aParams.myView, aOut.myExtract );

    aOut.myDrawCountBeforeCull = aOut.myExtract.myOpaque.myDrawInstances.size() + aOut.myExtract.myTransparent.myDrawInstances.size();
    aOut.myUnculledOpaque      = aOut.myExtract.myOpaque;

    if ( !aParams.myGpuCullEnabled ) {
        Gfx_CullDrawInstancesInPlace( *aParams.myScene, aParams.myView, aOut.myExtract.myOpaque );
        Gfx_CullDrawInstancesInPlace( *aParams.myScene, aParams.myView, aOut.myExtract.myTransparent );
    }
    if ( aParams.myLodEnabled ) {
        Gfx_ApplyLodToFrameExtract( *aParams.myScene, aParams.myCameraEye, *aParams.myLodTable, *aParams.myLodState, aOut.myExtract );
    }

    if ( aParams.myLodEnabled && !aLogs.myLodLoggedOnce ) {
        for ( const Gfx_DrawInstance& draw : aOut.myExtract.myOpaque.myDrawInstances ) {
            if ( aParams.myLodDebugLogicalMeshId == UINT32_MAX || aParams.myScene->GetLogicalMeshId( draw.myEntityIndex ) != aParams.myLodDebugLogicalMeshId ) {
                continue;
            }
            const glm::vec3 center = Gfx_BoundsCenter( aParams.myScene->GetBounds( draw.myEntityIndex ) );
            const float     dist   = glm::length( center - aParams.myCameraEye );
            UtilLogger::Info( "LOD", "slot=" + std::to_string( draw.myEntityIndex ) + " logical=" + std::to_string( aParams.myLodDebugLogicalMeshId )
                                         + " resolvedMeshId=" + std::to_string( draw.myMeshId ) + " dist=" + std::to_string( dist ) );
        }
        aLogs.myLodLoggedOnce = true;
    }

    Gfx_SortOpaqueDrawInstances( aOut.myExtract.myOpaque );
    Gfx_SortTransparentDrawInstances( aOut.myExtract.myTransparent, *aParams.myScene, aParams.myCameraView );
    Gfx_SortOpaqueDrawInstances( aOut.myUnculledOpaque );
    Gfx_BuildOpaqueDrawBatches( aOut.myExtract.myOpaque.myDrawInstances, aOut.myOpaqueBatchRuns );
    Gfx_BuildOpaqueDrawBatches( aOut.myExtract.myTransparent.myDrawInstances, aOut.myTransparentBatchRuns );
    Gfx_BuildOpaqueDrawBatches( aOut.myUnculledOpaque.myDrawInstances, aOut.myShadowCasterBatchRuns );

    if ( !aLogs.myBatchLoggedOnce ) {
        UtilLogger::Info( "BATCH",
                          "opaque runs=" + std::to_string( aOut.myOpaqueBatchRuns.size() ) + " draws=" + std::to_string( aOut.myExtract.myOpaque.myDrawInstances.size() ) );
        aLogs.myBatchLoggedOnce = true;
    }

    if ( !aLogs.myTransLoggedOnce ) {
        UtilLogger::Info( "TRANSP",
                          "runs=" + std::to_string( aOut.myTransparentBatchRuns.size() ) + " draws=" + std::to_string( aOut.myExtract.myTransparent.myDrawInstances.size() ) );
        aLogs.myTransLoggedOnce = true;
    }

    if ( !aLogs.myExtractLoggedOnce ) {
        UtilLogger::Info( "EXTRACT", "entities=" + std::to_string( aParams.myScene->GetActiveCount() ) + " draws=" + std::to_string( aOut.myDrawCountBeforeCull ) );
        const char* cullMode = aParams.myGpuCullEnabled ? "gpu-deferred" : "frustum+layer";
        UtilLogger::Info( "CULL", "opaque=" + std::to_string( aOut.myExtract.myOpaque.myDrawInstances.size() )
                                      + " transparent=" + std::to_string( aOut.myExtract.myTransparent.myDrawInstances.size() ) + " (" + cullMode + ")" );
        aLogs.myExtractLoggedOnce = true;
    }
}

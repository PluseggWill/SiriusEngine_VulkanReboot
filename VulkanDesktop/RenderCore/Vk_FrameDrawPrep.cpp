#include "Vk_FrameDrawPrep.h"

#include "../Util/Util_Logger.h"
#include "Vk_DescriptorPolicy.h"

#include <cstring>
#include <stdexcept>

void Vk_FrameDrawPrep::ClearFrameOutputs() {
    myExtract.myOpaque.myDrawInstances.clear();
    myExtract.myTransparent.myDrawInstances.clear();
    myOpaqueBatchRuns.clear();
    myTransparentBatchRuns.clear();
    myDrawCountBeforeCull = 0;
}

void Vk_FrameDrawPrep::ResetLogState() {
    Gfx_ResetFrameDrawStreamLogState( myStreamLogs );
    mySlabFillLoggedOnce         = false;
    myInstanceSlabOverflowLogged = false;
}

bool Vk_FrameDrawPrep::Build( const Vk_FrameDrawPrepBuildParams& aParams ) {
    Gfx_FrameDrawStreamParams streamParams{};
    streamParams.myScene                 = aParams.myScene;
    streamParams.myView.myView           = aParams.myCamera->myView;
    streamParams.myView.myProj           = aParams.myCamera->myProj;
    streamParams.myCameraEye             = aParams.myCamera->myEye;
    streamParams.myCameraView            = aParams.myCamera->myView;
    streamParams.myLodTable              = aParams.myLodTable;
    streamParams.myLodState              = aParams.myLodState;
    streamParams.myLodDebugLogicalMeshId = aParams.myLodDebugLogicalMeshId;

    Gfx_FrameDrawStreamOutput streamOut{};
    Gfx_BuildFrameDrawStream( streamParams, streamOut, myStreamLogs );

    myExtract              = std::move( streamOut.myExtract );
    myOpaqueBatchRuns      = std::move( streamOut.myOpaqueBatchRuns );
    myTransparentBatchRuns = std::move( streamOut.myTransparentBatchRuns );
    myDrawCountBeforeCull  = streamOut.myDrawCountBeforeCull;

    const bool slabOk = FillInstanceSlab( aParams );
    if ( !slabOk && !myInstanceSlabOverflowLogged ) {
        UtilLogger::Warn( "RESOURCE", "Skipping RecordScenePass: instance slab overflow (see FillInstanceSlab error)." );
        myInstanceSlabOverflowLogged = true;
    }

    return slabOk;
}

bool Vk_FrameDrawPrep::FillInstanceSlab( const Vk_FrameDrawPrepBuildParams& aParams ) {
    Vk_FrameData& frame = ( *aParams.myFrameDatas )[ aParams.myCurrentFrame ];
    if ( frame.myInstanceSlabMapped == nullptr ) {
        UtilLogger::Error( "RESOURCE", "Instance slab not mapped for frame " + std::to_string( aParams.myCurrentFrame ) );
        return false;
    }

    const size_t drawCount =
        myExtract.myOpaque.myDrawInstances.size() + myExtract.myTransparent.myDrawInstances.size();
    if ( drawCount > VkDescriptorPolicy::kMaxInstanceSlabEntries ) {
        UtilLogger::Error( "RESOURCE",
                           "Instance slab overflow: draws=" + std::to_string( drawCount ) + " max=" +
                               std::to_string( VkDescriptorPolicy::kMaxInstanceSlabEntries ) );
        return false;
    }

    char* const  slabBase = static_cast< char* >( frame.myInstanceSlabMapped );
    const size_t stride   = aParams.myInstanceSlabStride;
    size_t       writeIndex = 0;

    auto writeDrawList = [ & ]( Gfx_ExtractResult& aList ) {
        for ( Gfx_DrawInstance& draw : aList.myDrawInstances ) {
            draw.myInstanceDataOffset = static_cast< uint32_t >( writeIndex * stride );
            ++writeIndex;

            GpuObjectData objectData{};
            objectData.model         = aParams.myScene->GetTransform( draw.myEntityIndex );
            objectData.materialIndex = draw.myMaterialId;
            memcpy( slabBase + draw.myInstanceDataOffset, &objectData, sizeof( objectData ) );
        }
    };

    writeDrawList( myExtract.myOpaque );
    writeDrawList( myExtract.myTransparent );

    if ( !mySlabFillLoggedOnce ) {
        UtilLogger::Info( "RESOURCE", "FillInstanceSlab: wrote " + std::to_string( drawCount ) + " instance(s)" );
        mySlabFillLoggedOnce = true;
    }

    return true;
}

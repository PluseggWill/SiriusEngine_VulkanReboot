#include "Vk_FrameDrawPrep.h"

#include "../Util/Util_Logger.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_RenderBackend.h"

#include <cstring>
#include <stdexcept>

void Vk_FrameDrawPrep::ClearFrameOutputs() {
    myDrawCountBeforeCull = 0;
    myFramePacket         = Gfx_FrameRenderPacket{};
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

    myDrawCountBeforeCull = streamOut.myDrawCountBeforeCull;

    Gfx_BuildFrameRenderPacketFromStream( streamOut, myFramePacket );

    const bool slabOk = FillInstanceSlab( aParams, myFramePacket );
    if ( !slabOk && !myInstanceSlabOverflowLogged ) {
        UtilLogger::Warn( "RESOURCE", "Skipping RecordScenePass: instance slab overflow (see FillInstanceSlab error)." );
        myInstanceSlabOverflowLogged = true;
    }

    if ( !Vk_RenderBackend::ValidateFramePacket( myFramePacket ) ) {
        UtilLogger::Warn( "RENDER", "Frame render packet validation failed." );
    }

    return slabOk;
}

bool Vk_FrameDrawPrep::FillInstanceSlab( const Vk_FrameDrawPrepBuildParams& aParams, Gfx_FrameRenderPacket& aPacket ) {
    Vk_FrameData& frame = ( *aParams.myFrameDatas )[ aParams.myCurrentFrame ];
    if ( frame.myInstanceSlabMapped == nullptr ) {
        UtilLogger::Error( "RESOURCE", "Instance slab not mapped for frame " + std::to_string( aParams.myCurrentFrame ) );
        return false;
    }

    const size_t drawCount = aPacket.myOpaquePass.myDraws.size() + aPacket.myTransparentPass.myDraws.size();
    const uint32_t maxEntries = aParams.myInstanceSlabMaxEntries > 0 ? aParams.myInstanceSlabMaxEntries : VkDescriptorPolicy::kMaxInstanceSlabEntries;
    if ( drawCount > maxEntries ) {
        UtilLogger::Error( "RESOURCE",
                           "Instance slab overflow: draws=" + std::to_string( drawCount ) + " max=" + std::to_string( maxEntries ) );
        return false;
    }

    char* const  slabBase   = static_cast< char* >( frame.myInstanceSlabMapped );
    const size_t stride     = aParams.myInstanceSlabStride;
    size_t       writeIndex = 0;
    const size_t slabCapacityBytes = static_cast< size_t >( VkDescriptorPolicy::kMaxInstanceSlabEntries ) * stride;
    const size_t slabWriteEnd = aParams.myInstanceSlabBaseOffset + drawCount * stride;
    if ( slabWriteEnd > slabCapacityBytes ) {
        UtilLogger::Error( "RESOURCE", "Instance slab partition overflow: writeEnd=" + std::to_string( slabWriteEnd )
                                           + " capacity=" + std::to_string( slabCapacityBytes ) );
        return false;
    }

    auto writeDrawList = [ & ]( std::vector< Gfx_DrawInstance >& someDraws ) {
        for ( Gfx_DrawInstance& draw : someDraws ) {
            draw.myInstanceDataOffset = static_cast< uint32_t >( aParams.myInstanceSlabBaseOffset + writeIndex * stride );
            ++writeIndex;

            GpuObjectData objectData{};
            objectData.model         = aParams.myScene->GetWorldTransform( draw.myEntityIndex );
            objectData.materialIndex = draw.myMaterialId;
            memcpy( slabBase + draw.myInstanceDataOffset, &objectData, sizeof( objectData ) );
        }
    };

    writeDrawList( aPacket.myOpaquePass.myDraws );
    writeDrawList( aPacket.myTransparentPass.myDraws );

    if ( !mySlabFillLoggedOnce ) {
        UtilLogger::Info( "RESOURCE", "FillInstanceSlab: wrote " + std::to_string( drawCount ) + " instance(s)" );
        mySlabFillLoggedOnce = true;
    }

    return true;
}

// Module: Vk_FrameDrawPrep - GPU upload from Gfx_FrameRenderPacket (built in App/Gfx).
// Fill order: instance slab first (myInstanceDataOffset), then draw templates (indirect + SSBO metadata).
#include "Vk_FrameDrawPrep.h"

#include "../Gfx/Gfx_DrawTemplate.h"
#include "../Gfx/Gfx_EntityGpuRecord.h"
#include "../Gfx/Gfx_RenderPacket.h"
#include "../Util/Util_Logger.h"
#include "Vk_DescriptorPolicy.h"
#include "Vk_RenderBackend.h"
#include "Vk_ResourceTables.h"

#include <cstring>
#include <stdexcept>
#include <unordered_map>

void Vk_FrameDrawPrep::ClearFrameOutputs() {
    myDrawCountBeforeCull = 0;
    myFramePacket         = Gfx_FrameRenderPacket{};
}

void Vk_FrameDrawPrep::ResetLogState() {
    Gfx_ResetFrameDrawStreamLogState( myStreamLogs );
    mySlabFillLoggedOnce         = false;
    myInstanceSlabOverflowLogged = false;
    myDrawTemplateOverflowLogged = false;
    myEntityRecordFillLoggedOnce = false;
    myEntityRecordOverflowLogged = false;
}

bool Vk_FrameDrawPrep::UploadFromPacket( const Vk_FrameDrawPrepBuildParams& aParams, const Gfx_FrameRenderPacket& aPacket ) {
    myFramePacket         = aPacket;
    myDrawCountBeforeCull = 0;

    const bool slabOk = FillInstanceSlab( aParams, myFramePacket );
    if ( !slabOk && !myInstanceSlabOverflowLogged ) {
        UtilLogger::Warn( "RESOURCE", "Skipping RecordScenePass: instance slab overflow (see FillInstanceSlab error)." );
        myInstanceSlabOverflowLogged = true;
    }

    const bool templateOk = slabOk && FillDrawTemplates( aParams, myFramePacket );
    if ( slabOk && !templateOk && !myDrawTemplateOverflowLogged ) {
        UtilLogger::Warn( "RESOURCE", "Skipping RecordScenePass: draw-template buffer overflow (see FillDrawTemplates error)." );
        myDrawTemplateOverflowLogged = true;
    }

    if ( !Vk_RenderBackend::ValidateFramePacket( myFramePacket ) ) {
        UtilLogger::Warn( "RENDER", "Frame render packet validation failed." );
    }

    return slabOk && templateOk;
}

bool Vk_FrameDrawPrep::FillInstanceSlab( const Vk_FrameDrawPrepBuildParams& aParams, Gfx_FrameRenderPacket& aPacket ) {
    Vk_FrameData& frame = ( *aParams.myFrameDatas )[ aParams.myCurrentFrame ];
    if ( frame.myInstanceSlabMapped == nullptr ) {
        UtilLogger::Error( "RESOURCE", "Instance slab not mapped for frame " + std::to_string( aParams.myCurrentFrame ) );
        return false;
    }

    const size_t   slabUpperBound = aPacket.myShadowCasterPass.myDraws.size() + aPacket.myTransparentPass.myDraws.size();
    const uint32_t maxEntries     = aParams.myInstanceSlabMaxEntries > 0 ? aParams.myInstanceSlabMaxEntries : VkDescriptorPolicy::kMaxInstanceSlabEntries;
    if ( slabUpperBound > maxEntries ) {
        UtilLogger::Error( "RESOURCE", "Instance slab overflow: draws=" + std::to_string( slabUpperBound ) + " max=" + std::to_string( maxEntries ) );
        return false;
    }

    char* const  slabBase          = static_cast< char* >( frame.myInstanceSlabMapped );
    const size_t stride            = aParams.myInstanceSlabStride;
    size_t       writeIndex        = 0;
    const size_t slabCapacityBytes = static_cast< size_t >( VkDescriptorPolicy::kMaxInstanceSlabEntries ) * stride;
    const size_t slabWriteEnd      = aParams.myInstanceSlabBaseOffset + slabUpperBound * stride;
    if ( slabWriteEnd > slabCapacityBytes ) {
        UtilLogger::Error( "RESOURCE", "Instance slab partition overflow: writeEnd=" + std::to_string( slabWriteEnd ) + " capacity=" + std::to_string( slabCapacityBytes ) );
        return false;
    }

    std::unordered_map< uint32_t, uint32_t > entityOffsets;
    entityOffsets.reserve( aPacket.myOpaquePass.myDraws.size() + aPacket.myTransparentPass.myDraws.size() + aPacket.myShadowCasterPass.myDraws.size() );

    auto assignInstanceData = [ & ]( std::vector< Gfx_DrawInstance >& someDraws ) {
        for ( Gfx_DrawInstance& draw : someDraws ) {
            const auto existing = entityOffsets.find( draw.myEntityIndex );
            if ( existing != entityOffsets.end() ) {
                draw.myInstanceDataOffset = existing->second;
                continue;
            }

            draw.myInstanceDataOffset = static_cast< uint32_t >( aParams.myInstanceSlabBaseOffset + writeIndex * stride );
            entityOffsets.emplace( draw.myEntityIndex, draw.myInstanceDataOffset );
            ++writeIndex;

            GpuObjectData objectData{};
            objectData.model         = aParams.myScene->GetWorldTransform( draw.myEntityIndex );
            objectData.materialIndex = draw.myMaterialId;
            memcpy( slabBase + draw.myInstanceDataOffset, &objectData, sizeof( objectData ) );
        }
    };

    assignInstanceData( aPacket.myOpaquePass.myDraws );
    assignInstanceData( aPacket.myTransparentPass.myDraws );
    assignInstanceData( aPacket.myShadowCasterPass.myDraws );

    if ( !mySlabFillLoggedOnce ) {
        UtilLogger::Info( "RESOURCE", "FillInstanceSlab: wrote " + std::to_string( writeIndex ) + " instance(s)" );
        mySlabFillLoggedOnce = true;
    }

    return true;
}

// CONTRACT (M2 prep §A): parallel arrays in myDrawIndirectBuffer + myDrawTemplateBuffer; same slot layout as FillInstanceSlab.
bool Vk_FrameDrawPrep::FillDrawTemplates( const Vk_FrameDrawPrepBuildParams& aParams, Gfx_FrameRenderPacket& aPacket ) {
    if ( aParams.myResourceTables == nullptr ) {
        UtilLogger::Error( "RESOURCE", "FillDrawTemplates: resource tables not set." );
        return false;
    }

    Vk_FrameData& frame = ( *aParams.myFrameDatas )[ aParams.myCurrentFrame ];
    if ( frame.myDrawIndirectMapped == nullptr || frame.myDrawTemplateMapped == nullptr ) {
        UtilLogger::Error( "RESOURCE", "Draw-template buffers not mapped for frame " + std::to_string( aParams.myCurrentFrame ) );
        return false;
    }

    const size_t   drawCount  = aPacket.myOpaquePass.myDraws.size() + aPacket.myTransparentPass.myDraws.size();
    const uint32_t maxEntries = aParams.myDrawBufferMaxEntries > 0 ? aParams.myDrawBufferMaxEntries : VkDescriptorPolicy::kMaxDrawTemplateEntries;
    if ( drawCount > maxEntries ) {
        UtilLogger::Error( "RESOURCE", "Draw-template overflow: draws=" + std::to_string( drawCount ) + " max=" + std::to_string( maxEntries ) );
        return false;
    }

    auto* const    indirectBase = static_cast< Gfx_DrawIndirectCommand* >( frame.myDrawIndirectMapped );
    auto* const    templateBase = static_cast< Gfx_DrawTemplate* >( frame.myDrawTemplateMapped );
    const uint32_t baseIndex    = aParams.myDrawBufferBaseIndex;

    auto writeDrawList = [ & ]( const std::vector< Gfx_DrawInstance >& someDraws, uint32_t aPassOffset ) {
        for ( size_t drawIndex = 0; drawIndex < someDraws.size(); ++drawIndex ) {
            const Gfx_DrawInstance& draw = someDraws[ drawIndex ];
            const Gfx_Mesh&         mesh = aParams.myResourceTables->GetMesh( draw.myMeshId );
            const uint32_t          slot = Gfx_ComputeDrawBufferSlot( baseIndex, aPassOffset, static_cast< uint32_t >( drawIndex ) );

            Gfx_DrawTemplate drawTemplate{};
            Gfx_FillDrawTemplate( drawTemplate, draw, mesh.myIndexCount, draw.myInstanceDataOffset );
            templateBase[ slot ] = drawTemplate;
            indirectBase[ slot ] = drawTemplate.myIndirect;
        }
    };

    aPacket.myDrawBufferBaseIndex = baseIndex;
    writeDrawList( aPacket.myOpaquePass.myDraws, aPacket.myOpaquePass.myDrawBufferPassOffset );
    writeDrawList( aPacket.myTransparentPass.myDraws, aPacket.myTransparentPass.myDrawBufferPassOffset );

    if ( !myDrawTemplateFillLoggedOnce ) {
        UtilLogger::Info( "RESOURCE", "FillDrawTemplates: wrote " + std::to_string( drawCount ) + " draw template(s)" );
        myDrawTemplateFillLoggedOnce = true;
    }

    return true;
}

// CONTRACT (P3): Gfx_EntityGpuRecord[slot] mirrors SoA columns; inactive slots keep layerMask == 0.
bool Vk_FrameDrawPrep::FillEntityRecords( const Gfx_SceneSoA& aScene, const Vk_ResourceTables& aTables, const Gfx_EntityRecordLodParams& aLod, uint32_t aCurrentFrame,
                                          std::vector< Vk_FrameData >& aFrameDatas ) {
    Vk_FrameData& frame = aFrameDatas[ aCurrentFrame ];
    if ( frame.myEntityRecordMapped == nullptr ) {
        UtilLogger::Error( "RESOURCE", "Entity-record buffer not mapped for frame " + std::to_string( aCurrentFrame ) );
        return false;
    }

    const uint32_t slotCount = aScene.GetSlotCount();
    if ( slotCount > VkDescriptorPolicy::kMaxEntitySlots ) {
        if ( !myEntityRecordOverflowLogged ) {
            UtilLogger::Error( "RESOURCE", "Entity-record overflow: slots=" + std::to_string( slotCount ) + " max=" + std::to_string( VkDescriptorPolicy::kMaxEntitySlots ) );
            myEntityRecordOverflowLogged = true;
        }
        return false;
    }

    Gfx_LodState              lodSnapshot{};
    Gfx_EntityRecordLodParams lodParams = aLod;
    if ( aLod.myLodEnabled && aLod.myLodState != nullptr ) {
        lodSnapshot          = *aLod.myLodState;
        lodParams.myLodState = &lodSnapshot;
    }

    auto* const recordBase = static_cast< Gfx_EntityGpuRecord* >( frame.myEntityRecordMapped );
    for ( uint32_t slot = 0; slot < slotCount; ++slot ) {
        const uint32_t meshId     = Gfx_ResolveEntityRecordMeshId( aScene, slot, lodParams );
        const uint32_t indexCount = aScene.IsSlotActive( slot ) ? aTables.GetMesh( meshId ).myIndexCount : 0u;
        Gfx_FillEntityGpuRecord( recordBase[ slot ], aScene, slot, indexCount );
    }

    if ( slotCount < VkDescriptorPolicy::kMaxEntitySlots ) {
        std::memset( recordBase + slotCount, 0, static_cast< size_t >( VkDescriptorPolicy::kMaxEntitySlots - slotCount ) * sizeof( Gfx_EntityGpuRecord ) );
    }

    if ( !myEntityRecordFillLoggedOnce ) {
        UtilLogger::Info( "RESOURCE", "FillEntityRecords: wrote " + std::to_string( slotCount ) + " slot(s)" );
        myEntityRecordFillLoggedOnce = true;
    }

    return true;
}

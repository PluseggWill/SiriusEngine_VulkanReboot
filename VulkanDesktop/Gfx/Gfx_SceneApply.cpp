#include "Gfx_SceneApply.h"

#include "../Util/Util_Logger.h"

#include <stdexcept>
#include <unordered_map>

namespace {

uint32_t LookupId( const std::unordered_map< std::string, uint32_t >& aTable, const std::string& aKey, const char* aKind ) {
    const auto it = aTable.find( aKey );
    if ( it == aTable.end() ) {
        throw std::runtime_error( std::string( "[SCENE] Unknown " ) + aKind + " id '" + aKey + "'" );
    }
    return it->second;
}

}  // namespace

Gfx_SceneIdTables Gfx_BuildSceneIdTables( const Gfx_SceneDesc& aScene ) {
    Gfx_SceneIdTables tables{};

    for ( size_t i = 0; i < aScene.myLogicalMeshes.size(); ++i ) {
        tables.myLogicalMeshIdByName.emplace( aScene.myLogicalMeshes[ i ].myId, static_cast< uint32_t >( i ) );
    }
    for ( size_t i = 0; i < aScene.myMeshes.size(); ++i ) {
        tables.myMeshIdByName.emplace( aScene.myMeshes[ i ].myId, static_cast< uint32_t >( i ) );
    }
    for ( size_t i = 0; i < aScene.myTextures.size(); ++i ) {
        tables.myTextureIdByName.emplace( aScene.myTextures[ i ].myId, static_cast< uint32_t >( i ) );
    }
    for ( size_t i = 0; i < aScene.myMaterials.size(); ++i ) {
        tables.myMaterialIdByName.emplace( aScene.myMaterials[ i ].myId, static_cast< uint32_t >( i ) );
    }

    return tables;
}

Gfx_SceneShaderPair Gfx_GetSceneShader( const Gfx_SceneDesc& aScene, const std::string& aShaderId ) {
    const auto it = aScene.myShaders.find( aShaderId );
    if ( it == aScene.myShaders.end() ) {
        throw std::runtime_error( "[SCENE] Unknown shader id '" + aShaderId + "'" );
    }
    return it->second;
}

void Gfx_BuildResourceManifestFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_ResourceManifest& aOut ) {
    ( void )aTables;
    aOut.myMeshes.clear();
    aOut.myTextures.clear();
    aOut.myMaterials.clear();

    for ( size_t i = 0; i < aScene.myMeshes.size(); ++i ) {
        aOut.myMeshes.push_back( { static_cast< uint32_t >( i ), aScene.myMeshes[ i ].myPath } );
    }

    for ( size_t i = 0; i < aScene.myTextures.size(); ++i ) {
        aOut.myTextures.push_back( { static_cast< uint32_t >( i ), aScene.myTextures[ i ].myPath } );
    }

    for ( size_t i = 0; i < aScene.myMaterials.size(); ++i ) {
        const Gfx_SceneMaterialEntry& src = aScene.myMaterials[ i ];
        Gfx_MaterialManifestEntry     dst{};
        dst.myId                    = static_cast< uint32_t >( i );
        dst.myTextureId             = LookupId( aTables.myTextureIdByName, src.myTextureId, "texture" );
        dst.myPipelinePermutationId = 0;
        dst.myBaseColorFactor       = src.myBaseColorFactor;
        dst.myRoughness             = src.myRoughness;
        dst.myMetallic              = src.myMetallic;
        dst.myAlpha                 = src.myAlpha;
        dst.myAlphaMode             = src.myAlphaMode;
        dst.myIsTransparent         = src.myIsTransparent;
        aOut.myMaterials.push_back( dst );
    }

    UtilLogger::Info( "SCENE", "Built resource manifest from scene: meshes=" + std::to_string( aOut.myMeshes.size() ) + " textures=" + std::to_string( aOut.myTextures.size() )
                                   + " materials=" + std::to_string( aOut.myMaterials.size() ) );
}

void Gfx_BuildLodTableFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_LodTable& aOut ) {
    aOut = Gfx_LodTable{};

    for ( size_t logicalIndex = 0; logicalIndex < aScene.myLogicalMeshes.size(); ++logicalIndex ) {
        const Gfx_SceneLogicalMeshEntry& logical = aScene.myLogicalMeshes[ logicalIndex ];
        if ( logical.myLodMeshes.empty() ) {
            throw std::runtime_error( "[SCENE] logicalMeshes['" + logical.myId + "'] requires at least one lodMeshes entry" );
        }

        Gfx_LodChain chain{};
        chain.myMeshIds.reserve( logical.myLodMeshes.size() );
        for ( const std::string& meshName : logical.myLodMeshes ) {
            chain.myMeshIds.push_back( LookupId( aTables.myMeshIdByName, meshName, "mesh" ) );
        }
        chain.myDistanceThresholds = logical.myLodDistances;

        if ( chain.myDistanceThresholds.size() >= chain.myMeshIds.size() ) {
            throw std::runtime_error( "[SCENE] logicalMeshes['" + logical.myId + "']: lodDistances count must be less than lodMeshes count" );
        }

        aOut.SetChain( static_cast< uint32_t >( logicalIndex ), std::move( chain ) );
    }

    UtilLogger::Info( "SCENE", "Built LOD table: logicalMeshes=" + std::to_string( aScene.myLogicalMeshes.size() ) );
}

void Gfx_PopulateSceneSoAFromSceneDesc( const Gfx_SceneDesc& aScene, const Gfx_SceneIdTables& aTables, Gfx_SceneSoA& aSceneSoA, Gfx_SceneTransformState& aTransformState ) {
    aSceneSoA.Clear();
    aTransformState.Clear();

    for ( const Gfx_SceneEntityEntry& entity : aScene.myEntities ) {
        const uint32_t logicalMeshId = LookupId( aTables.myLogicalMeshIdByName, entity.myLogicalMeshId, "logicalMesh" );
        const uint32_t materialId    = LookupId( aTables.myMaterialIdByName, entity.myMaterialId, "material" );

        const Gfx_StableEntityId id = aSceneSoA.AllocEntity( logicalMeshId, materialId, entity.myTransform, entity.myLayerMask, entity.myRenderFlags, entity.myLodBias );
        if ( id.myIndex >= aTransformState.mySourceWorldTransforms.size() ) {
            aTransformState.mySourceWorldTransforms.resize( id.myIndex + 1, glm::mat4( 1.0f ) );
        }
        if ( id.myIndex >= aTransformState.myResolvedWorldTransforms.size() ) {
            aTransformState.myResolvedWorldTransforms.resize( id.myIndex + 1, glm::mat4( 1.0f ) );
        }
        aTransformState.mySourceWorldTransforms[ id.myIndex ]   = entity.myTransform;
        aTransformState.myResolvedWorldTransforms[ id.myIndex ] = entity.myTransform;
    }

    UtilLogger::Info( "SCENE", "Populated SoA from scene: active entities=" + std::to_string( aSceneSoA.GetActiveCount() ) );
}

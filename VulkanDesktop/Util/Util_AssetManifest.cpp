#include "Util_AssetManifest.h"

#include "Util_Logger.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

void AddPath( std::vector< Util_AssetManifestEntry >& aOut, std::unordered_set< std::string >& aSeen, const std::string& aPath,
              Util_AssetKind aKind ) {
    if ( aPath.empty() || aSeen.find( aPath ) != aSeen.end() ) {
        return;
    }
    aSeen.insert( aPath );
    aOut.push_back( { aPath, aKind } );
}

const Gfx_SceneShaderPair* FindShader( const Gfx_SceneDesc& aScene, const std::string& aShaderId ) {
    const auto it = aScene.myShaders.find( aShaderId );
    return it != aScene.myShaders.end() ? &it->second : nullptr;
}

std::unordered_map< std::string, std::string > BuildTexturePathById( const Gfx_SceneDesc& aScene ) {
    std::unordered_map< std::string, std::string > paths;
    paths.reserve( aScene.myTextures.size() );
    for ( const Gfx_SceneTextureEntry& texture : aScene.myTextures ) {
        paths.emplace( texture.myId, texture.myPath );
    }
    return paths;
}

}  // namespace

Util_AssetManifest Util_CollectDependencies( const Gfx_SceneDesc& aScene ) {
    Util_AssetManifest                manifest{};
    std::unordered_set< std::string > seenPaths;

    for ( const auto& [ shaderId, shaderPair ] : aScene.myShaders ) {
        (void)shaderId;
        AddPath( manifest.myEntries, seenPaths, shaderPair.myVertPath, Util_AssetKind::ShaderVert );
        AddPath( manifest.myEntries, seenPaths, shaderPair.myFragPath, Util_AssetKind::ShaderFrag );
    }

    for ( const Gfx_SceneMeshEntry& mesh : aScene.myMeshes ) {
        AddPath( manifest.myEntries, seenPaths, mesh.myPath, Util_AssetKind::Mesh );
    }

    for ( const Gfx_SceneTextureEntry& texture : aScene.myTextures ) {
        AddPath( manifest.myEntries, seenPaths, texture.myPath, Util_AssetKind::Texture );
    }

    const std::unordered_map< std::string, std::string > texturePaths = BuildTexturePathById( aScene );
    for ( const Gfx_SceneMaterialEntry& material : aScene.myMaterials ) {
        const Gfx_SceneShaderPair* shader = FindShader( aScene, material.myShaderId );
        if ( shader == nullptr ) {
            throw std::runtime_error( "[SCENE] Material '" + material.myId + "' references unknown shader '" + material.myShaderId + "'" );
        }
        AddPath( manifest.myEntries, seenPaths, shader->myVertPath, Util_AssetKind::ShaderVert );
        AddPath( manifest.myEntries, seenPaths, shader->myFragPath, Util_AssetKind::ShaderFrag );

        const auto textureIt = texturePaths.find( material.myTextureId );
        if ( textureIt == texturePaths.end() ) {
            throw std::runtime_error( "[SCENE] Material '" + material.myId + "' references unknown texture '" + material.myTextureId + "'" );
        }
        AddPath( manifest.myEntries, seenPaths, textureIt->second, Util_AssetKind::Texture );
    }

    std::sort( manifest.myEntries.begin(), manifest.myEntries.end(),
               []( const Util_AssetManifestEntry& aLeft, const Util_AssetManifestEntry& aRight ) {
                   return aLeft.myLogicalPath < aRight.myLogicalPath;
               } );

    UtilLogger::Info( "SCENE", "Collected " + std::to_string( manifest.myEntries.size() ) + " unique asset path(s) from scene manifest" );
    return manifest;
}

std::vector< std::string > Util_CollectDependencyPaths( const Gfx_SceneDesc& aScene ) {
    const Util_AssetManifest   manifest = Util_CollectDependencies( aScene );
    std::vector< std::string > paths;
    paths.reserve( manifest.myEntries.size() );
    for ( const Util_AssetManifestEntry& entry : manifest.myEntries ) {
        paths.push_back( entry.myLogicalPath );
    }
    return paths;
}

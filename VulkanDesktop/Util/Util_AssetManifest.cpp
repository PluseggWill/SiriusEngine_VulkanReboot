#include "Util_AssetManifest.h"

#include "Util_Loader.h"
#include "Util_Logger.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

void AddPath( std::vector< Util_AssetManifestEntry >& aOut, std::unordered_set< std::string >& aSeen, const std::string& aPath, Util_AssetKind aKind ) {
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
        ( void )shaderId;
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
               []( const Util_AssetManifestEntry& aLeft, const Util_AssetManifestEntry& aRight ) { return aLeft.myLogicalPath < aRight.myLogicalPath; } );

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

void Util_VerifyManifest( const Util_EngineConfig& aConfig, const Util_AssetManifest& aManifest, Util_AssetVerifyPolicy aPolicy ) {
    const char* policyLabel = aPolicy == Util_AssetVerifyPolicy::Strict ? "strict" : "warn";
    UtilLogger::Info( "STARTUP", "Verifying scene asset manifest (" + std::to_string( aManifest.myEntries.size() ) + " path(s), policy=" + policyLabel + ")." );

    std::vector< std::string > missing;
    for ( const Util_AssetManifestEntry& entry : aManifest.myEntries ) {
        const std::string&          logical  = entry.myLogicalPath;
        const std::string           resolved = UtilLoader::ResolvePath( aConfig, logical );
        const std::filesystem::path path     = resolved;
        if ( !std::filesystem::exists( path ) || !std::filesystem::is_regular_file( path ) ) {
            missing.push_back( logical + " (resolved: " + resolved + ")" );
            if ( aPolicy == Util_AssetVerifyPolicy::Warn ) {
                UtilLogger::Warn( "STARTUP", "Missing optional file: " + logical + " (resolved: " + resolved + ")" );
            }
            else {
                UtilLogger::Error( "STARTUP", "Missing required file: " + logical + " (resolved: " + resolved + ")" );
            }
        }
        else {
            UtilLogger::Info( "STARTUP", "OK " + logical + " -> " + resolved );
        }
    }

    if ( !missing.empty() ) {
        if ( aPolicy == Util_AssetVerifyPolicy::Warn ) {
            UtilLogger::Warn( "STARTUP", "Manifest verify (warn): " + std::to_string( missing.size() ) + " missing path(s); continuing. First: " + missing.front() );
            return;
        }
        throw std::runtime_error( "Missing " + std::to_string( missing.size() ) + " required asset(s). First: " + missing.front() );
    }

    UtilLogger::Info( "STARTUP", "All scene manifest assets present." );
}

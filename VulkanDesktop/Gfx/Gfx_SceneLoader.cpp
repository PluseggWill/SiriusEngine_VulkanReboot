#include "Gfx_SceneLoader.h"

#include "../Util/Util_Loader.h"
#include "../Util/Util_Logger.h"

#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {

using Json = nlohmann::json;

void RequireObject( const Json& aJson, const char* aLabel ) {
    if ( !aJson.is_object() ) {
        throw std::runtime_error( std::string( "[SCENE] Expected object for " ) + aLabel );
    }
}

void RequireArray( const Json& aJson, const char* aLabel ) {
    if ( !aJson.is_array() ) {
        throw std::runtime_error( std::string( "[SCENE] Expected array for " ) + aLabel );
    }
}

std::string RequireString( const Json& aJson, const char* aField, const char* aContext ) {
    if ( !aJson.contains( aField ) || !aJson[ aField ].is_string() ) {
        throw std::runtime_error( std::string( "[SCENE] Missing or invalid string '" ) + aField + "' in " + aContext );
    }
    return aJson[ aField ].get< std::string >();
}

void RequireUniqueId( std::unordered_set< std::string >& aSeen, const std::string& aId, const char* aSection ) {
    if ( !aSeen.insert( aId ).second ) {
        throw std::runtime_error( std::string( "[SCENE] Duplicate id '" ) + aId + "' in " + aSection );
    }
}

// CONTRACT: transform is 16 floats in glm column-major order (same as glm::value_ptr(mat4)).
glm::mat4 ParseTransform( const Json& aJson, const char* aContext ) {
    RequireArray( aJson, aContext );
    if ( aJson.size() != 16 ) {
        throw std::runtime_error( std::string( "[SCENE] transform must have 16 floats in " ) + aContext );
    }
    std::array< float, 16 > values{};
    for ( size_t i = 0; i < 16; ++i ) {
        if ( !aJson[ i ].is_number() ) {
            throw std::runtime_error( std::string( "[SCENE] transform entry must be numeric in " ) + aContext );
        }
        values[ i ] = aJson[ i ].get< float >();
    }
    return glm::make_mat4( values.data() );
}

Gfx_RenderFlags ParseRenderFlags( const Json& aEntityJson ) {
    if ( !aEntityJson.contains( "renderFlags" ) ) {
        return Gfx_RenderOpaque;
    }
    const Json& flags = aEntityJson[ "renderFlags" ];
    if ( flags.is_string() ) {
        const std::string value = flags.get< std::string >();
        if ( value == "opaque" ) {
            return Gfx_RenderOpaque;
        }
        if ( value == "transparent" ) {
            return Gfx_RenderTransparent;
        }
        throw std::runtime_error( "[SCENE] Unknown renderFlags string: " + value );
    }
    if ( flags.is_number_unsigned() ) {
        return static_cast< Gfx_RenderFlags >( flags.get< uint32_t >() );
    }
    throw std::runtime_error( "[SCENE] renderFlags must be string or unsigned integer" );
}

void ParseShaders( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "shaders" ) ) {
        return;
    }
    RequireObject( aRoot[ "shaders" ], "shaders" );
    for ( auto it = aRoot[ "shaders" ].begin(); it != aRoot[ "shaders" ].end(); ++it ) {
        RequireObject( it.value(), ( "shader '" + it.key() + "'" ).c_str() );
        Gfx_SceneShaderPair pair{};
        pair.myVertPath = RequireString( it.value(), "vert", it.key().c_str() );
        pair.myFragPath = RequireString( it.value(), "frag", it.key().c_str() );
        aOut.myShaders.emplace( it.key(), std::move( pair ) );
    }
}

void ParseMeshes( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "meshes" ) ) {
        return;
    }
    RequireArray( aRoot[ "meshes" ], "meshes" );
    std::unordered_set< std::string > seenIds;
    for ( const Json& entry : aRoot[ "meshes" ] ) {
        RequireObject( entry, "meshes[]" );
        Gfx_SceneMeshEntry mesh{};
        mesh.myId   = RequireString( entry, "id", "meshes[]" );
        mesh.myPath = RequireString( entry, "path", "meshes[]" );
        RequireUniqueId( seenIds, mesh.myId, "meshes" );
        aOut.myMeshes.push_back( std::move( mesh ) );
    }
}

void ParseTextures( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "textures" ) ) {
        return;
    }
    RequireArray( aRoot[ "textures" ], "textures" );
    std::unordered_set< std::string > seenIds;
    for ( const Json& entry : aRoot[ "textures" ] ) {
        RequireObject( entry, "textures[]" );
        Gfx_SceneTextureEntry texture{};
        texture.myId   = RequireString( entry, "id", "textures[]" );
        texture.myPath = RequireString( entry, "path", "textures[]" );
        RequireUniqueId( seenIds, texture.myId, "textures" );
        aOut.myTextures.push_back( std::move( texture ) );
    }
}

void ParseLogicalMeshes( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "logicalMeshes" ) ) {
        return;
    }
    RequireArray( aRoot[ "logicalMeshes" ], "logicalMeshes" );
    std::unordered_set< std::string > seenIds;
    for ( const Json& entry : aRoot[ "logicalMeshes" ] ) {
        RequireObject( entry, "logicalMeshes[]" );
        Gfx_SceneLogicalMeshEntry logical{};
        logical.myId = RequireString( entry, "id", "logicalMeshes[]" );
        RequireUniqueId( seenIds, logical.myId, "logicalMeshes" );

        if ( !entry.contains( "lodMeshes" ) ) {
            throw std::runtime_error( "[SCENE] logicalMeshes[] requires lodMeshes array" );
        }
        RequireArray( entry[ "lodMeshes" ], "logicalMeshes[].lodMeshes" );
        for ( const Json& meshRef : entry[ "lodMeshes" ] ) {
            if ( !meshRef.is_string() ) {
                throw std::runtime_error( "[SCENE] logicalMeshes[].lodMeshes entries must be strings" );
            }
            logical.myLodMeshes.push_back( meshRef.get< std::string >() );
        }

        if ( entry.contains( "lodDistances" ) ) {
            RequireArray( entry[ "lodDistances" ], "logicalMeshes[].lodDistances" );
            for ( const Json& distance : entry[ "lodDistances" ] ) {
                if ( !distance.is_number() ) {
                    throw std::runtime_error( "[SCENE] lodDistances entries must be numeric" );
                }
                logical.myLodDistances.push_back( distance.get< float >() );
            }
        }

        aOut.myLogicalMeshes.push_back( std::move( logical ) );
    }
}

void ParseMaterials( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "materials" ) ) {
        return;
    }
    RequireArray( aRoot[ "materials" ], "materials" );
    std::unordered_set< std::string > seenIds;
    for ( const Json& entry : aRoot[ "materials" ] ) {
        RequireObject( entry, "materials[]" );
        Gfx_SceneMaterialEntry material{};
        material.myId        = RequireString( entry, "id", "materials[]" );
        material.myShaderId  = RequireString( entry, "shader", "materials[]" );
        material.myTextureId = RequireString( entry, "texture", "materials[]" );
        RequireUniqueId( seenIds, material.myId, "materials" );
        if ( entry.contains( "alpha" ) ) {
            if ( !entry[ "alpha" ].is_number() ) {
                throw std::runtime_error( "[SCENE] materials[].alpha must be numeric" );
            }
            material.myAlpha = entry[ "alpha" ].get< float >();
        }
        if ( entry.contains( "transparent" ) ) {
            if ( !entry[ "transparent" ].is_boolean() ) {
                throw std::runtime_error( "[SCENE] materials[].transparent must be boolean" );
            }
            material.myIsTransparent = entry[ "transparent" ].get< bool >();
        }
        aOut.myMaterials.push_back( std::move( material ) );
    }
}

void ParseEntities( const Json& aRoot, Gfx_SceneDesc& aOut ) {
    if ( !aRoot.contains( "entities" ) ) {
        return;
    }
    RequireArray( aRoot[ "entities" ], "entities" );
    for ( size_t i = 0; i < aRoot[ "entities" ].size(); ++i ) {
        const Json& entry = aRoot[ "entities" ][ i ];
        RequireObject( entry, "entities[]" );
        const std::string context = "entities[" + std::to_string( i ) + "]";

        Gfx_SceneEntityEntry entity{};
        entity.myLogicalMeshId = RequireString( entry, "logicalMesh", context.c_str() );
        entity.myMaterialId    = RequireString( entry, "material", context.c_str() );
        if ( !entry.contains( "transform" ) ) {
            throw std::runtime_error( "[SCENE] Missing transform in " + context );
        }
        entity.myTransform   = ParseTransform( entry[ "transform" ], context.c_str() );
        entity.myRenderFlags = ParseRenderFlags( entry );
        if ( entry.contains( "layerMask" ) ) {
            if ( !entry[ "layerMask" ].is_number_unsigned() ) {
                throw std::runtime_error( "[SCENE] layerMask must be unsigned integer in " + context );
            }
            entity.myLayerMask = entry[ "layerMask" ].get< uint32_t >();
        }
        if ( entry.contains( "lodBias" ) ) {
            if ( !entry[ "lodBias" ].is_number() ) {
                throw std::runtime_error( "[SCENE] lodBias must be numeric in " + context );
            }
            entity.myLodBias = entry[ "lodBias" ].get< float >();
        }
        aOut.myEntities.push_back( std::move( entity ) );
    }
}

}  // namespace

Gfx_SceneDesc Gfx_LoadSceneDesc( const std::string& aLogicalPath ) {
    const std::string resolved = UtilLoader::ResolvePath( aLogicalPath );
    UtilLogger::Info( "SCENE", "Loading scene: " + aLogicalPath + " -> " + resolved );

    const std::vector< char > bytes = UtilLoader::ReadFile( aLogicalPath );
    const std::string         text( bytes.begin(), bytes.end() );

    Json root;
    try {
        root = Json::parse( text );
    }
    catch ( const Json::parse_error& ex ) {
        throw std::runtime_error( std::string( "[SCENE] JSON parse error in " ) + aLogicalPath + ": " + ex.what() );
    }

    RequireObject( root, "root" );

    Gfx_SceneDesc scene{};
    if ( !root.contains( "version" ) || !root[ "version" ].is_number_unsigned() ) {
        throw std::runtime_error( "[SCENE] Missing or invalid version (expected unsigned integer)" );
    }
    scene.myVersion = root[ "version" ].get< uint32_t >();
    if ( scene.myVersion != kGfxSceneFormatVersion ) {
        throw std::runtime_error( "[SCENE] Unsupported scene version " + std::to_string( scene.myVersion ) + " (expected " + std::to_string( kGfxSceneFormatVersion ) + ")" );
    }

    if ( root.contains( "name" ) ) {
        if ( !root[ "name" ].is_string() ) {
            throw std::runtime_error( "[SCENE] name must be a string" );
        }
        scene.myName = root[ "name" ].get< std::string >();
    }

    ParseShaders( root, scene );
    ParseLogicalMeshes( root, scene );
    ParseMeshes( root, scene );
    ParseTextures( root, scene );
    ParseMaterials( root, scene );
    ParseEntities( root, scene );

    UtilLogger::Info( "SCENE", "Parsed scene v" + std::to_string( scene.myVersion ) + " name='" + scene.myName
                                   + "' logicalMeshes=" + std::to_string( scene.myLogicalMeshes.size() ) + " meshes=" + std::to_string( scene.myMeshes.size() )
                                   + " textures=" + std::to_string( scene.myTextures.size() ) + " materials=" + std::to_string( scene.myMaterials.size() )
                                   + " entities=" + std::to_string( scene.myEntities.size() ) );
    return scene;
}

#include "Gfx_ShaderPermutation.h"

#include "../Util/Util_Logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <unordered_map>

namespace {

using Json = nlohmann::json;

bool                                        gInitialized = false;
std::vector< Gfx_ShaderPermutationDef >     gDefinitions;
std::unordered_map< std::string, uint32_t > gIdByName;
uint32_t                                    gActiveId = 0;

uint32_t FeatureFromName( const std::string& aName ) {
    if ( aName == "ALPHA_CLIP" ) {
        return Gfx_ShaderFeature_AlphaClip;
    }
    if ( aName == "SHADOWS" ) {
        return Gfx_ShaderFeature_Shadows;
    }
    if ( aName == "IBL" ) {
        return Gfx_ShaderFeature_Ibl;
    }
    throw std::runtime_error( "Gfx_ShaderPermutation: unknown feature '" + aName + "'" );
}

}  // namespace

namespace Gfx_ShaderPermutation {

uint32_t ParseFeatureMask( const std::vector< std::string >& aFeatureNames ) {
    uint32_t mask = 0;
    for ( const std::string& name : aFeatureNames ) {
        mask |= FeatureFromName( name );
    }
    return mask;
}

uint16_t EncodeSortKeyPermSlot( uint16_t aShaderPermutationId, uint16_t aMaterialTableGeneration ) {
    if ( aShaderPermutationId == 0 ) {
        return aMaterialTableGeneration;
    }
    return static_cast< uint16_t >( ( ( aShaderPermutationId & 0xFFu ) << 8 ) | ( aMaterialTableGeneration & 0xFFu ) );
}

void Initialize( const std::string& aResolvedRegistryPath ) {
    if ( gInitialized ) {
        return;
    }

    std::ifstream file( aResolvedRegistryPath );
    if ( !file.is_open() ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: cannot open registry: " + aResolvedRegistryPath );
    }

    Json root;
    try {
        file >> root;
    }
    catch ( const Json::exception& e ) {
        throw std::runtime_error( std::string( "Gfx_ShaderPermutation: invalid JSON: " ) + aResolvedRegistryPath + " - " + e.what() );
    }

    if ( !root.contains( "permutations" ) || !root[ "permutations" ].is_array() ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: missing permutations[] in " + aResolvedRegistryPath );
    }

    gDefinitions.clear();
    gIdByName.clear();

    for ( const Json& entry : root[ "permutations" ] ) {
        Gfx_ShaderPermutationDef def{};
        def.myId   = entry.at( "id" ).get< uint32_t >();
        def.myName = entry.at( "name" ).get< std::string >();
        if ( entry.contains( "features" ) && entry[ "features" ].is_array() ) {
            for ( const Json& feature : entry[ "features" ] ) {
                def.myFeatureMask |= FeatureFromName( feature.get< std::string >() );
            }
        }
        if ( entry.contains( "glslcDefines" ) && entry[ "glslcDefines" ].is_array() ) {
            for ( const Json& define : entry[ "glslcDefines" ] ) {
                def.myGlslcDefines.push_back( define.get< std::string >() );
            }
        }
        def.myVertSpvLogicalPath = entry.at( "vertSpv" ).get< std::string >();
        def.myFragSpvLogicalPath = entry.at( "fragSpv" ).get< std::string >();

        if ( gIdByName.find( def.myName ) != gIdByName.end() ) {
            throw std::runtime_error( "Gfx_ShaderPermutation: duplicate permutation name '" + def.myName + "'" );
        }
        gIdByName.emplace( def.myName, def.myId );
        gDefinitions.push_back( std::move( def ) );
    }

    // M7 (#17): PermutationRegistry.json stays lit + lit_alpha_clip until hybrid pass 2.
    if ( gDefinitions.size() > kFrozenRegistryPermutationMax ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: registry has " + std::to_string( gDefinitions.size() ) + " permutation(s); M7 freeze allows at most "
                                  + std::to_string( kFrozenRegistryPermutationMax ) + " until hybrid pass 2" );
    }
    for ( const Gfx_ShaderPermutationDef& def : gDefinitions ) {
        if ( ( def.myFeatureMask & ~kFrozenRegistryAllowedFeatureMask ) != 0 ) {
            throw std::runtime_error( "Gfx_ShaderPermutation: permutation '" + def.myName + "' uses frozen feature bits (M7); only ALPHA_CLIP allowed until hybrid pass 2" );
        }
    }

    gInitialized = true;
    // Default active = first registry entry until App calls SetActiveByName.
    gActiveId = gDefinitions.empty() ? 0u : gDefinitions.front().myId;

    UtilLogger::Info( "SHADER-PERM", "registry loaded: " + std::to_string( gDefinitions.size() ) + " permutation(s) from " + aResolvedRegistryPath );
}

bool IsInitialized() {
    return gInitialized;
}

const std::vector< Gfx_ShaderPermutationDef >& GetDefinitions() {
    if ( !gInitialized ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: Initialize() not called" );
    }
    return gDefinitions;
}

uint32_t GetActiveId() {
    if ( !gInitialized ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: Initialize() not called" );
    }
    return gActiveId;
}

const Gfx_ShaderPermutationDef& GetDefinition( uint32_t aId ) {
    for ( const Gfx_ShaderPermutationDef& def : GetDefinitions() ) {
        if ( def.myId == aId ) {
            return def;
        }
    }
    throw std::runtime_error( "Gfx_ShaderPermutation: unknown id " + std::to_string( aId ) );
}

const Gfx_ShaderPermutationDef& GetActiveDefinition() {
    return GetDefinition( GetActiveId() );
}

void SetActiveByName( const std::string& aName ) {
    if ( !gInitialized ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: Initialize() not called" );
    }
    const auto it = gIdByName.find( aName );
    if ( it == gIdByName.end() ) {
        throw std::runtime_error( "Gfx_ShaderPermutation: unknown permutation '" + aName + "'" );
    }
    gActiveId                           = it->second;
    const Gfx_ShaderPermutationDef& def = GetDefinition( gActiveId );
    UtilLogger::Info( "SHADER-PERM", "active permutation name=" + def.myName + " id=" + std::to_string( def.myId ) + " featureMask=0x" + std::to_string( def.myFeatureMask )
                                         + " fragSpv=" + def.myFragSpvLogicalPath );
}

}  // namespace Gfx_ShaderPermutation

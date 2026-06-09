// Module: ShaderReflect — offline SPIR-V descriptor reflection + contract validation (S2 phase 2a).
// Not linked into VulkanDesktop; invoked from ReflectShaders_Lit.bat after glslc.
// CLI: --spv <path>@<vertex|fragment> uses @ so Windows drive letters (D:) are not parsed as separators.

#include <nlohmann/json.hpp>

#include <spirv_reflect.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using json = nlohmann::json;

constexpr int kExitOk            = 0;
constexpr int kExitSpvError      = 1;
constexpr int kExitContractError = 2;
constexpr int kExitArgsError     = 3;

struct SpvInput {
    std::string myPath;
    std::string myStageLabel;  // vertex | fragment
};

struct MergedBinding {
    uint32_t                   mySet     = 0;
    uint32_t                   myBinding = 0;
    SpvReflectDescriptorType   myType    = SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uint32_t                   myCount   = 1;
    std::string                myName;
    uint32_t                   myStageFlags = 0;
    std::vector< std::string > mySeenIn;
};

uint64_t BindingKey( uint32_t aSet, uint32_t aBinding ) {
    return ( static_cast< uint64_t >( aSet ) << 32 ) | aBinding;
}

// Stable repo-relative path for committed reflection JSON (host absolute paths drift per machine).
std::string LogicalSpvPath( const std::string& aHostPath ) {
    const size_t      sep      = aHostPath.find_last_of( "/\\" );
    const std::string fileName = ( sep == std::string::npos ) ? aHostPath : aHostPath.substr( sep + 1 );
    return "VulkanDesktop/Shader_Generated/" + fileName;
}

const char* DescriptorTypeName( SpvReflectDescriptorType aType ) {
    switch ( aType ) {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return "SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return "COMBINED_IMAGE_SAMPLER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return "SAMPLED_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return "STORAGE_IMAGE";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return "UNIFORM_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return "STORAGE_TEXEL_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return "UNIFORM_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return "STORAGE_BUFFER";
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return "UNIFORM_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return "STORAGE_BUFFER_DYNAMIC";
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return "INPUT_ATTACHMENT";
    default:
        return "UNKNOWN";
    }
}

uint32_t StageLabelToFlags( const std::string& aLabel ) {
    if ( aLabel == "vertex" || aLabel == "VERTEX" ) {
        return SPV_REFLECT_SHADER_STAGE_VERTEX_BIT;
    }
    if ( aLabel == "fragment" || aLabel == "FRAGMENT" ) {
        return SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT;
    }
    return 0;
}

std::vector< uint8_t > ReadFileBytes( const std::string& aPath, std::string& aError ) {
    std::ifstream file( aPath, std::ios::binary | std::ios::ate );
    if ( !file ) {
        aError = "cannot open: " + aPath;
        return {};
    }
    const std::streamsize size = file.tellg();
    if ( size <= 0 ) {
        aError = "empty file: " + aPath;
        return {};
    }
    file.seekg( 0, std::ios::beg );
    std::vector< uint8_t > bytes( static_cast< size_t >( size ) );
    if ( !file.read( reinterpret_cast< char* >( bytes.data() ), size ) ) {
        aError = "read failed: " + aPath;
        return {};
    }
    return bytes;
}

bool ReflectSpv( const SpvInput& aInput, std::unordered_map< uint64_t, MergedBinding >& aMerged, std::string& aError ) {
    std::string                  readError;
    const std::vector< uint8_t > bytes = ReadFileBytes( aInput.myPath, readError );
    if ( bytes.empty() ) {
        aError = readError;
        return false;
    }

    SpvReflectShaderModule module{};
    const SpvReflectResult createResult = spvReflectCreateShaderModule( bytes.size(), bytes.data(), &module );
    if ( createResult != SPV_REFLECT_RESULT_SUCCESS ) {
        aError = "spvReflectCreateShaderModule failed for " + aInput.myPath;
        return false;
    }

    uint32_t bindingCount = 0;
    if ( spvReflectEnumerateDescriptorBindings( &module, &bindingCount, nullptr ) != SPV_REFLECT_RESULT_SUCCESS ) {
        spvReflectDestroyShaderModule( &module );
        aError = "enumerate count failed: " + aInput.myPath;
        return false;
    }

    std::vector< SpvReflectDescriptorBinding* > bindings( bindingCount );
    if ( bindingCount > 0 ) {
        if ( spvReflectEnumerateDescriptorBindings( &module, &bindingCount, bindings.data() ) != SPV_REFLECT_RESULT_SUCCESS ) {
            spvReflectDestroyShaderModule( &module );
            aError = "enumerate bindings failed: " + aInput.myPath;
            return false;
        }
    }

    const uint32_t stageFlags = StageLabelToFlags( aInput.myStageLabel );
    if ( stageFlags == 0 ) {
        spvReflectDestroyShaderModule( &module );
        aError = "unknown stage label: " + aInput.myStageLabel;
        return false;
    }

    const std::string seenName = aInput.myPath.substr( aInput.myPath.find_last_of( "/\\" ) + 1 );

    for ( SpvReflectDescriptorBinding* binding : bindings ) {
        if ( binding == nullptr ) {
            continue;
        }
        const uint64_t key  = BindingKey( binding->set, binding->binding );
        MergedBinding& slot = aMerged[ key ];
        if ( slot.mySeenIn.empty() ) {
            slot.mySet        = binding->set;
            slot.myBinding    = binding->binding;
            slot.myType       = binding->descriptor_type;
            slot.myCount      = binding->count;
            slot.myName       = binding->name ? binding->name : "";
            slot.myStageFlags = stageFlags;
            slot.mySeenIn.push_back( seenName );
        }
        else {
            // Cross-stage merge (lit_batch): same set+binding in vert and frag → stageFlags |= .
            if ( slot.myType != binding->descriptor_type ) {
                spvReflectDestroyShaderModule( &module );
                aError = "type conflict set " + std::to_string( binding->set ) + " binding " + std::to_string( binding->binding );
                return false;
            }
            slot.myStageFlags |= stageFlags;
            slot.mySeenIn.push_back( seenName );
        }
    }

    spvReflectDestroyShaderModule( &module );
    return true;
}

json BuildReflectionJson( const std::string& aGroup, const std::vector< SpvInput >& aInputs, const std::unordered_map< uint64_t, MergedBinding >& aMerged ) {
    json root;
    root[ "schemaVersion" ] = 1;
    root[ "pipelineGroup" ] = aGroup;
    root[ "shaders" ]       = json::array();
    for ( const SpvInput& input : aInputs ) {
        json shaderEntry;
        shaderEntry[ "spv" ]        = LogicalSpvPath( input.myPath );
        shaderEntry[ "stage" ]      = input.myStageLabel;
        shaderEntry[ "entryPoint" ] = "main";
        root[ "shaders" ].push_back( std::move( shaderEntry ) );
    }

    std::unordered_map< uint32_t, std::vector< const MergedBinding* > > bySet;
    for ( const auto& pair : aMerged ) {
        bySet[ pair.second.mySet ].push_back( &pair.second );
    }

    json                    setsJson = json::array();
    std::vector< uint32_t > setNumbers;
    setNumbers.reserve( bySet.size() );
    for ( const auto& pair : bySet ) {
        setNumbers.push_back( pair.first );
    }
    std::sort( setNumbers.begin(), setNumbers.end() );

    for ( uint32_t setNumber : setNumbers ) {
        json setEntry;
        setEntry[ "set" ]      = setNumber;
        setEntry[ "bindings" ] = json::array();

        std::vector< const MergedBinding* > bindings = bySet[ setNumber ];
        std::sort( bindings.begin(), bindings.end(), []( const MergedBinding* aLeft, const MergedBinding* aRight ) { return aLeft->myBinding < aRight->myBinding; } );

        for ( const MergedBinding* binding : bindings ) {
            json bindingEntry;
            bindingEntry[ "binding" ]        = binding->myBinding;
            bindingEntry[ "name" ]           = binding->myName;
            bindingEntry[ "descriptorType" ] = DescriptorTypeName( binding->myType );
            bindingEntry[ "count" ]          = binding->myCount;
            bindingEntry[ "stages" ]         = json::array();
            if ( binding->myStageFlags & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT ) {
                bindingEntry[ "stages" ].push_back( "VERTEX" );
            }
            if ( binding->myStageFlags & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT ) {
                bindingEntry[ "stages" ].push_back( "FRAGMENT" );
            }
            bindingEntry[ "seenIn" ] = binding->mySeenIn;
            setEntry[ "bindings" ].push_back( std::move( bindingEntry ) );
        }
        setsJson.push_back( std::move( setEntry ) );
    }

    root[ "sets" ] = std::move( setsJson );
    return root;
}

uint32_t ContractStagesToFlags( const json& aStages ) {
    uint32_t flags = 0;
    for ( const json& stage : aStages ) {
        flags |= StageLabelToFlags( stage.get< std::string >() );
    }
    return flags;
}

// CONTRACT: DescriptorContract_*.json must match Vk_Enum.h / GLSL. SPIR-V reports UNIFORM_BUFFER;
// UNIFORM_BUFFER_DYNAMIC is alloc policy only (VkDescriptorPolicy Set 2) — not a separate SPIR-V type.
bool ValidateContract( const json& aReflectRoot, const json& aContract, std::vector< std::string >& aErrors ) {
    if ( !aContract.contains( "sets" ) || !aReflectRoot.contains( "sets" ) ) {
        aErrors.push_back( "contract or reflection missing sets[]" );
        return false;
    }

    auto findReflectBinding = [ & ]( uint32_t aSet, uint32_t aBinding ) -> const json* {
        for ( const json& setEntry : aReflectRoot[ "sets" ] ) {
            if ( setEntry[ "set" ].get< uint32_t >() != aSet ) {
                continue;
            }
            for ( const json& bindingEntry : setEntry[ "bindings" ] ) {
                if ( bindingEntry[ "binding" ].get< uint32_t >() == aBinding ) {
                    return &bindingEntry;
                }
            }
        }
        return nullptr;
    };

    bool ok = true;
    for ( const json& contractSet : aContract[ "sets" ] ) {
        const uint32_t setNumber = contractSet[ "set" ].get< uint32_t >();
        for ( const json& contractBinding : contractSet[ "bindings" ] ) {
            const uint32_t bindingNumber  = contractBinding[ "binding" ].get< uint32_t >();
            const json*    reflectBinding = findReflectBinding( setNumber, bindingNumber );
            if ( reflectBinding == nullptr ) {
                aErrors.push_back( "missing in SPIR-V: set " + std::to_string( setNumber ) + " binding " + std::to_string( bindingNumber ) );
                ok = false;
                continue;
            }

            const std::string contractType = contractBinding[ "descriptorType" ].get< std::string >();
            const std::string reflectType  = ( *reflectBinding )[ "descriptorType" ].get< std::string >();
            if ( contractType != reflectType ) {
                aErrors.push_back( "set " + std::to_string( setNumber ) + " binding " + std::to_string( bindingNumber ) + ": expected " + contractType + ", got "
                                   + reflectType );
                ok = false;
            }

            const uint32_t requiredStages = ContractStagesToFlags( contractBinding[ "stages" ] );
            uint32_t       reflectStages  = 0;
            for ( const json& stage : ( *reflectBinding )[ "stages" ] ) {
                reflectStages |= StageLabelToFlags( stage.get< std::string >() );
            }
            if ( ( reflectStages & requiredStages ) != requiredStages ) {
                aErrors.push_back( "set " + std::to_string( setNumber ) + " binding " + std::to_string( bindingNumber ) + ": stages mismatch (contract requires "
                                   + contractBinding[ "stages" ].dump() + ")" );
                ok = false;
            }
        }

        // Extra bindings in reflect not listed in contract.
        for ( const json& reflectSet : aReflectRoot[ "sets" ] ) {
            if ( reflectSet[ "set" ].get< uint32_t >() != setNumber ) {
                continue;
            }
            for ( const json& reflectBinding : reflectSet[ "bindings" ] ) {
                const uint32_t bindingNumber = reflectBinding[ "binding" ].get< uint32_t >();
                bool           found         = false;
                for ( const json& contractBinding : contractSet[ "bindings" ] ) {
                    if ( contractBinding[ "binding" ].get< uint32_t >() == bindingNumber ) {
                        found = true;
                        break;
                    }
                }
                if ( !found ) {
                    aErrors.push_back( "extra in SPIR-V: set " + std::to_string( setNumber ) + " binding " + std::to_string( bindingNumber ) );
                    ok = false;
                }
            }
        }
    }

    return ok;
}

void PrintUsage() {
    std::cerr << "ShaderReflect --group <name> --spv <path>@<vertex|fragment> ... --contract <json> --out <json>\n";
}

bool ParseArgs( int argc, char** argv, std::string& aGroup, std::vector< SpvInput >& aInputs, std::string& aContractPath, std::string& aOutPath ) {
    for ( int i = 1; i < argc; ++i ) {
        const std::string arg = argv[ i ];
        if ( arg == "--group" && i + 1 < argc ) {
            aGroup = argv[ ++i ];
        }
        else if ( arg == "--spv" && i + 1 < argc ) {
            const std::string value  = argv[ ++i ];
            const size_t      atSign = value.rfind( '@' );
            if ( atSign == std::string::npos || atSign == 0 || atSign + 1 >= value.size() ) {
                return false;
            }
            SpvInput input{};
            input.myPath       = value.substr( 0, atSign );
            input.myStageLabel = value.substr( atSign + 1 );
            aInputs.push_back( std::move( input ) );
        }
        else if ( arg == "--contract" && i + 1 < argc ) {
            aContractPath = argv[ ++i ];
        }
        else if ( arg == "--out" && i + 1 < argc ) {
            aOutPath = argv[ ++i ];
        }
        else {
            return false;
        }
    }
    return !aGroup.empty() && !aInputs.empty() && !aContractPath.empty() && !aOutPath.empty();
}

}  // namespace

int main( int argc, char** argv ) {
    std::string             group;
    std::vector< SpvInput > inputs;
    std::string             contractPath;
    std::string             outPath;
    if ( !ParseArgs( argc, argv, group, inputs, contractPath, outPath ) ) {
        PrintUsage();
        return kExitArgsError;
    }

    std::unordered_map< uint64_t, MergedBinding > merged;
    for ( const SpvInput& input : inputs ) {
        std::string error;
        if ( !ReflectSpv( input, merged, error ) ) {
            std::cerr << "[SHADER-REFLECT] " << error << "\n";
            return kExitSpvError;
        }
    }

    const json reflectionRoot = BuildReflectionJson( group, inputs, merged );

    std::ifstream contractFile( contractPath );
    if ( !contractFile ) {
        std::cerr << "[SHADER-REFLECT] cannot open contract: " << contractPath << "\n";
        return kExitContractError;
    }
    json contractRoot;
    try {
        contractFile >> contractRoot;
    }
    catch ( const json::exception& ex ) {
        std::cerr << "[SHADER-REFLECT] contract JSON parse error: " << ex.what() << "\n";
        return kExitContractError;
    }

    std::vector< std::string > errors;
    if ( !ValidateContract( reflectionRoot, contractRoot, errors ) ) {
        for ( const std::string& line : errors ) {
            std::cerr << "[SHADER-REFLECT] " << line << "\n";
        }
        return kExitContractError;
    }

    std::ofstream outFile( outPath );
    if ( !outFile ) {
        std::cerr << "[SHADER-REFLECT] cannot write: " << outPath << "\n";
        return kExitSpvError;
    }
    outFile << reflectionRoot.dump( 2 ) << "\n";

    std::cerr << "[SHADER-REFLECT] " << group << " OK (" << merged.size() << " bindings, contract " << contractPath << ")\n";
    return kExitOk;
}

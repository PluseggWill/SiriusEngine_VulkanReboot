#pragma once

#include <cstdint>
#include <string>
#include <vector>

// S2 permutation registry: feature bits + offline SPIR-V variants (see Shader/PermutationRegistry.json).

enum Gfx_ShaderFeatureBit : uint32_t {
    Gfx_ShaderFeature_None      = 0,
    Gfx_ShaderFeature_AlphaClip = 1u << 0,
    Gfx_ShaderFeature_Shadows   = 1u << 1,
    Gfx_ShaderFeature_Ibl       = 1u << 2,
    Gfx_ShaderFeature_Pbr       = 1u << 3,  // Reserved (Stage 2); no registry entry yet.
};

struct Gfx_ShaderPermutationDef {
    uint32_t                   myId = 0;
    std::string                myName;
    uint32_t                   myFeatureMask = 0;
    std::vector< std::string > myGlslcDefines;
    std::string                myVertSpvLogicalPath;
    std::string                myFragSpvLogicalPath;
};

namespace Gfx_ShaderPermutation {

inline constexpr const char* kRegistryLogicalPath = "VulkanDesktop/Shader/PermutationRegistry.json";

void Initialize();
bool IsInitialized();

const std::vector< Gfx_ShaderPermutationDef >& GetDefinitions();

uint32_t                        GetActiveId();
const Gfx_ShaderPermutationDef& GetActiveDefinition();
const Gfx_ShaderPermutationDef& GetDefinition( uint32_t aId );

// Select active permutation by registry name (from engine.json / CLI). Call after Initialize().
void SetActiveByName( const std::string& aName );

uint32_t ParseFeatureMask( const std::vector< std::string >& aFeatureNames );

// Sort-key high 16 bits: when aShaderPermutationId==0, entire slot is aMaterialTableGeneration (bindless v0 compat).
// Otherwise: low byte = table generation (<256), high byte = shader permutation id.
uint16_t EncodeSortKeyPermSlot( uint16_t aShaderPermutationId, uint16_t aMaterialTableGeneration );

}  // namespace Gfx_ShaderPermutation

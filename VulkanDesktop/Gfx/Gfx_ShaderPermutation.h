#pragma once

#include "Gfx_ShaderPermutationTypes.h"

#include <cstdint>
#include <string>
#include <vector>

// S2 permutation registry runtime API (see Shader/PermutationRegistry.json).
// Type-only consumers should include Gfx_ShaderPermutationTypes.h.
// App resolves the registry path + active name from Util_EngineConfig; Gfx does not take EngineConfig.

namespace Gfx_ShaderPermutation {

void Initialize( const std::string& aResolvedRegistryPath );
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

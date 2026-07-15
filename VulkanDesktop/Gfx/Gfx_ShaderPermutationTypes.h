#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Shader permutation data contract: feature bits + offline SPIR-V variant descriptors.
// Runtime registry loading remains in Gfx_ShaderPermutation.*.
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

// POLICY_BINDLESS M7 (#17): freeze registry until hybrid pass 2 needs a shader branch.
inline constexpr uint32_t kFrozenRegistryPermutationMax     = 2u;
inline constexpr uint32_t kFrozenRegistryAllowedFeatureMask = Gfx_ShaderFeature_AlphaClip;

}  // namespace Gfx_ShaderPermutation

#pragma once

#include <string>

// Stage 1 render presets (Active-Plan / forward-rendering-epic). Map to PermutationRegistry.json names.
namespace Gfx_RenderPreset {

// Returns registry permutation name (e.g. ForwardLit -> "lit"). Throws on unknown preset.
std::string ToShaderPermutationName( const std::string& aRenderPreset );

}  // namespace Gfx_RenderPreset

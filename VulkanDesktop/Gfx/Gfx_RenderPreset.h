#pragma once

#include <string>

// Stage 1/2 render presets (Active-Plan). Map to PermutationRegistry.json for scene material table load.
namespace Gfx_RenderPreset {

// Returns registry permutation name (e.g. ForwardLit -> "lit").
// HybridDeferred -> "lit" for scene resources; geometry shading uses GBuffer*.spv (Vk_GBufferPass).
std::string ToShaderPermutationName( const std::string& aRenderPreset );

// True when --render-preset / config selects the hybrid FG path (not a permutation id).
bool IsHybridDeferred( const std::string& aRenderPreset );

}  // namespace Gfx_RenderPreset

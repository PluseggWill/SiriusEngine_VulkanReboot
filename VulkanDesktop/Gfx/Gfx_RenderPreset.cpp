#include "Gfx_RenderPreset.h"

#include <stdexcept>
#include <unordered_map>

namespace {

const std::unordered_map< std::string, std::string > kPresetToPermutation = {
    { "ForwardLit", "lit" },
    { "ForwardLitAlphaClip", "lit_alpha_clip" },
    { "HybridDeferred", "lit" },
};

}  // namespace

namespace Gfx_RenderPreset {

bool IsHybridDeferred( const std::string& aRenderPreset ) {
    return aRenderPreset == "HybridDeferred";
}

std::string ToShaderPermutationName( const std::string& aRenderPreset ) {
    const auto it = kPresetToPermutation.find( aRenderPreset );
    if ( it == kPresetToPermutation.end() ) {
        throw std::runtime_error( "Gfx_RenderPreset: unknown render preset '" + aRenderPreset + "' (supported: ForwardLit, ForwardLitAlphaClip, HybridDeferred)" );
    }
    return it->second;
}

}  // namespace Gfx_RenderPreset

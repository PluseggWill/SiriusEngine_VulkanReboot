#pragma once

// Session-only render/debug toggles passed App → RenderCore (no App headers in record path).
struct Gfx_FrameDebugToggles {
    bool mySkipOpaquePass      = false;
    bool mySkipTransparentPass = false;
    bool myLodEnabled          = false;
};

inline Gfx_FrameDebugToggles Gfx_FrameDebugTogglesFromRenderDebug( bool aSkipOpaque, bool aSkipTransparent, bool aLodEnabled ) {
    Gfx_FrameDebugToggles toggles{};
    toggles.mySkipOpaquePass      = aSkipOpaque;
    toggles.mySkipTransparentPass = aSkipTransparent;
    toggles.myLodEnabled          = aLodEnabled;
    return toggles;
}

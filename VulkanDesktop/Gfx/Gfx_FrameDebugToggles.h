#pragma once

// Session + frame policy toggles passed App → RenderCore (no App / EngineConfig on record path).
struct Gfx_FrameDebugToggles {
    bool mySkipOpaquePass      = false;
    bool mySkipTransparentPass = false;
    bool myLodEnabled          = false;
    bool myGpuCullEnabled      = false;
    bool myLegacyDirectDraw    = false;
    bool myHybridDeferred      = false;
};

inline Gfx_FrameDebugToggles Gfx_FrameDebugTogglesFromApp( bool aSkipOpaque, bool aSkipTransparent, bool aLodEnabled, bool aGpuCullEnabled, bool aLegacyDirectDraw,
                                                           bool aHybridDeferred ) {
    Gfx_FrameDebugToggles toggles{};
    toggles.mySkipOpaquePass      = aSkipOpaque;
    toggles.mySkipTransparentPass = aSkipTransparent;
    toggles.myLodEnabled          = aLodEnabled;
    toggles.myGpuCullEnabled      = aGpuCullEnabled;
    toggles.myLegacyDirectDraw    = aLegacyDirectDraw;
    toggles.myHybridDeferred      = aHybridDeferred;
    return toggles;
}

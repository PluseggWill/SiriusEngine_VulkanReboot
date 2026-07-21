#pragma once

#include <chrono>

// Narrow frame callbacks for Platform ↔ RenderCore (no Vk_Renderer in Pf_PlatformHost API).
struct Pf_FrameHooks {
    using FrameStartFn = void ( * )( void* aUser, std::chrono::high_resolution_clock::time_point aFrameStart, float aDeltaSeconds );
    using VoidFn       = void ( * )( void* aUser );

    void*        myUser                 = nullptr;
    FrameStartFn myOnFrameStart         = nullptr;
    VoidFn       myOnImGuiNewFrame      = nullptr;
    VoidFn       myOnFramebufferResized = nullptr;
};

#pragma once

#include "Gfx_PassId.h"

#include <cstdint>
#include <vector>

// Declarative frame graph plan owned by Gfx (topology + enable). Record stays in RenderCore until E4.
struct Gfx_FramePlanNode {
    Gfx_PassId                myId = Gfx_PassId::Count;
    std::vector< Gfx_PassId > myDependencies;
    bool                      myEnabled = true;
};

struct Gfx_FramePlan {
    std::vector< Gfx_FramePlanNode > myNodes;
    std::vector< Gfx_PassId >        myOrdered;  // topological order over myNodes
};

// Runtime enable bits filled by App/RenderCore from pass Init + settings (no Vulkan types).
struct Gfx_PipelineEnableFlags {
    bool myShadow              = false;
    bool myGBuffer             = true;
    bool myClusterBuild        = true;
    bool myDepthPyramid        = false;
    bool mySsr                 = false;
    bool myAo                  = false;
    bool myDdgiProbeUpdate     = false;
    bool myShadowAoSoft        = false;
    bool myDeferredTransparent = true;
    bool myPost                = false;
};

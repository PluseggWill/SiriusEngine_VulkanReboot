#pragma once

#include "Gfx_AoSettings.h"
#include "Gfx_LightingGlobals.h"
#include "Gfx_PassId.h"

#include <glm/vec3.hpp>

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

// Final per-pass run bits consumed by BuildHybridDeferred / FrameGraph Execute.
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

// Resource readiness only (no Vulkan types). Filled by RenderCore/App from pass Init state.
struct Gfx_PassResourceReady {
    bool myShadowMap         = false;
    bool myDepthPyramid      = false;
    bool mySsr               = false;
    bool myAo                = false;
    bool myShadowAoSoft      = false;
    bool myDeferredLighting  = false;
    bool myPostHybridResolve = false;
};

// Settings + readiness + sun — Gfx resolves policy into Gfx_PipelineEnableFlags.
struct Gfx_PipelineBuildInput {
    Gfx_LightingSettings  myLighting{};
    Gfx_AoSettings        myAo{};
    glm::vec3             mySunDirectionTowardLight{ 0.0f, 0.0f, 1.0f };
    Gfx_PassResourceReady myReady{};
};

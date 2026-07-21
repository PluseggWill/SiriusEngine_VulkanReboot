#pragma once

#include "Gfx_FramePlan.h"

#include <string>

// Module: Gfx_RenderPipeline — build declarative FramePlan (no Vulkan / no RenderCore).
namespace Gfx_RenderPipeline {

// Policy: settings + resource readiness → enable flags (sun elevation / AO toggles / DDGI, etc.).
Gfx_PipelineEnableFlags ResolveEnableFlags( const Gfx_PipelineBuildInput& aInput );

// HybridDeferred topology (matches historical Vk_FrameGraph BuildHybridDeferredNodes deps).
Gfx_FramePlan BuildHybridDeferred( const Gfx_PipelineEnableFlags& aEnable );
Gfx_FramePlan BuildHybridDeferred( const Gfx_PipelineBuildInput& aInput );

// Once-per-process log string for FG (no side effects).
std::string HybridDeferredChainLabel();

}  // namespace Gfx_RenderPipeline

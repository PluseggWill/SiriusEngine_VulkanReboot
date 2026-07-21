#pragma once

#include "Gfx_FramePlan.h"

#include <string>

// Module: Gfx_RenderPipeline — build declarative FramePlan (no Vulkan / no RenderCore).
namespace Gfx_RenderPipeline {

// HybridDeferred topology (matches historical Vk_FrameGraph BuildHybridDeferredNodes deps).
Gfx_FramePlan BuildHybridDeferred( const Gfx_PipelineEnableFlags& aEnable );

// Once-per-process log string for FG (no side effects).
std::string HybridDeferredChainLabel();

}  // namespace Gfx_RenderPipeline
